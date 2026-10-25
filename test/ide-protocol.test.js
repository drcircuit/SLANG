import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { Decoder } from '../ide/protocol.cjs';
const root = path.resolve('build/ide-protocol-tests');
await mkdir(root, { recursive: true });
const executable = process.env.SLANG_BINARY || path.resolve(`build/native-ninja/slang${process.platform === 'win32' ? '.exe' : ''}`);
async function run(name, source, action, debug = false) {
  const file = path.join(root, `${name}.slang`); await writeFile(file, source);
  return new Promise((resolve, reject) => {
    const child = spawn(executable, ['ide', file, ...(debug ? ['--debug'] : [])], { windowsHide: true, env: { ...process.env, SDL_VIDEO_DRIVER: 'dummy', SDL_RENDER_DRIVER: 'software' } });
    const events = [], decoder = new Decoder(); let stderr = '';
    const timer = setTimeout(() => { child.kill(); reject(new Error(`Runner timed out: ${name}`)); }, 10000);
    const send = command => child.stdin.write(command + '\n');
    child.stdin.on('error', () => {});
    decoder.on('packet', e => {
      events.push(e);
      try { action?.(e, send, file); if (e.type === 'ready') send('start'); if (e.type === 'frame') send('frame-ack'); }
      catch (error) { child.kill(); reject(error); }
    });
    child.stdout.on('data', data => { try { decoder.push(data); } catch (e) { child.kill(); reject(e); } });
    child.stderr.on('data', data => { stderr += data; });
    child.on('error', reject);
    child.on('close', code => { clearTimeout(timer); try { decoder.end(); resolve({ events, code, stderr }); } catch (e) { reject(e); } });
  });
}
test('decoder tolerates arbitrary byte boundaries and rejects malformed lengths', () => {
  const data = Buffer.from(JSON.stringify({ type: 'console', text: 'Hei æøå 🎉' }));
  const length = Buffer.alloc(4); length.writeUInt32LE(data.length);
  const decoder = new Decoder(), result = []; decoder.on('packet', e => result.push(e));
  for (const byte of Buffer.concat([length, data])) decoder.push(Buffer.from([byte]));
  decoder.end(); assert.equal(result[0].text, 'Hei æøå 🎉');
  assert.throws(() => new Decoder().push(Buffer.from([255,255,255,255])), /size/);
});
test('console and input preserve Unicode', async () => {
  const result = await run('input', 'var name = input<"Name? ">\nprint<name>\n', (e, send) => {
    if (e.type === 'state' && e.state === 'input') send(`input\t${Buffer.from('Ærlig Ørn').toString('base64')}`);
  });
  assert.equal(result.code, 0, result.stderr);
  assert.match(result.events.filter(e => e.type === 'console').map(e => e.text).join(''), /Name\? Ærlig Ørn/);
});
test('debug steps, inspects variables and reaches breakpoints', async () => {
  const pauses = [];
  const result = await run('debug', 'var x = 1\nx = x + 2\nprint<x>\n', (e, send, file) => {
    if (e.type === 'ready') send(`breakpoints\t${Buffer.from(file).toString('base64')}\t3`);
    if (e.type === 'paused') { pauses.push(e); send(pauses.length === 1 ? 'step' : 'resume'); }
  }, true);
  assert.equal(result.code, 0); assert.deepEqual(pauses.map(p => p.line), [1,2,3]);
  assert.equal(pauses[1].variables.find(v => v.name === 'x').value, '1');
  assert.equal(pauses[2].variables.find(v => v.name === 'x').value, '3');
  assert.equal(pauses[2].reason, 'breakpoint');
});
test('stop works while waiting for input', async () => {
  const result = await run('stop', 'input<>\n', (e, send) => { if (e.state === 'input') send('stop'); });
  assert.equal(result.events.at(-1).status, 'stopped');
});
test('pause, resume and stop work during a library sleep', async () => {
  let paused = false;
  const result = await run('sleep-control', 'use time\nprint<"sleeping">\ntime.sleep<10>\nerror<"Sleep was not stopped">', (e, send) => {
    if (e.type === 'console' && e.text.includes('sleeping')) send('pause');
    if (e.type === 'paused') { paused = true; send('resume'); }
    if (e.state === 'running' && paused) send('stop');
  });
  assert.equal(paused, true);
  assert.equal(result.events.at(-1).status, 'stopped');
});
test('standard libraries work together through the IDE runner', async () => {
  const result = await run('libraries', 'use files\nuse strings as s\nuse numbers as n\nuse lists\nuse random\nrandom.seed<12>\nvar øre = 0.01\nfiles.write<"ide-notes.txt", s.upper<"blåbær">>\nassert<files.read<"ide-notes.txt"> is "BLÅBÆR">\nvar values = lists.sort<list<3, 1, 2>>\nassert<random.choice<values> >= 1>\nprint<n.sum<values>, øre>');
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  assert.equal(result.events.filter(e => e.type === 'console').map(e => e.text).join(''), '6 0.01\n');
});
test('pausing console input restores the input prompt after resume', async () => {
  let prompts = 0;
  const result = await run('pause-input', 'print<input<>>\n', (e, send) => {
    if (e.state === 'input') { if (++prompts === 1) send('pause'); else send(`input\t${Buffer.from('continued').toString('base64')}`); }
    if (e.type === 'paused') send('resume');
  });
  assert.equal(prompts, 2); assert.equal(result.code, 0);
  assert.ok(result.events.some(e => e.text === 'continued\n'));
});
test('keyboard events reach the SDL frame loop', async () => {
  const result = await run('keys', 'use graphics as g\ng.canvas<16, 16>\nwhile g.open<>:\n if g.key_down<"right">:\n  print<"right pressed">\n  g.close<>\n  break\n .\n g.present<>\n.\n', (e, send) => {
    if (e.type === 'ready') send('key\tRight\t1');
  });
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  assert.ok(result.events.some(e => e.text === 'right pressed\n'));
});
test('runtime errors carry source locations', async () => {
  const result = await run('error', 'var a = 1\nerror<"Oops">\n');
  const error = result.events.find(e => e.type === 'error');
  assert.equal(result.code, 1); assert.equal(error.line, 2); assert.match(error.message, /Oops/);
});
test('SDL frames are embedded as RGBA and static drawings finish', async () => {
  const result = await run('canvas', 'use graphics as g\ng.canvas<32, 24>\ng.background<"#FF0000">\n');
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  const frame = result.events.filter(e => e.type === 'frame').at(-1);
  assert.equal(frame.width, 32); assert.equal(frame.height, 24);
  assert.deepEqual([...frame.pixels.subarray(0, 4)], [255,0,0,255]);
});
