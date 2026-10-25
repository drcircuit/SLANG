import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { Decoder } from '../ide/protocol.cjs';
const root = path.resolve('build/window-protocol-tests'); await mkdir(root, { recursive: true });
const executable = process.env.SLANG_BINARY || path.resolve(`build/native-ninja/slang${process.platform === 'win32' ? '.exe' : ''}`);
async function run(name, source, action) {
  const file = path.join(root, name + '.slang'); await writeFile(file, source);
  return new Promise((resolve, reject) => {
    const child = spawn(executable, ['ide', file], { windowsHide: true, env: { ...process.env, SDL_VIDEO_DRIVER: 'dummy', SDL_RENDER_DRIVER: 'software' } });
    const events = [], decoder = new Decoder(); let stderr = '', frames = 0, failure;
    const timer = setTimeout(() => { child.kill(); reject(new Error(`Window test timed out: ${name}`)); }, 10000);
    const send = line => child.stdin.write(line + '\n');
    child.stdin.on('error', () => {});
    child.on('error', error => { clearTimeout(timer); reject(error); });
    child.stderr.on('data', data => { stderr += data; });
    child.stdout.on('data', bytes => { try { decoder.push(bytes); } catch (e) { failure = e; child.kill(); } });
    decoder.on('packet', event => {
      try {
        if (event.type === 'frame') frames++; else events.push(event);
        action?.(event, send, frames);
        if (event.type === 'ready') send('start');
        if (event.type === 'frame') send('frame-ack');
      } catch (e) { failure = e; child.kill(); }
    });
    child.on('close', code => {
      clearTimeout(timer);
      try { decoder.end(); if (failure) throw failure; resolve({ code, frames, events, stderr }); } catch (e) { reject(e); }
    });
  });
}
const key = (send, name, shift = false, control = false) => send(`window-key\t${name}\t${+shift}\t${+control}`);
const text = (send, value) => send(`window-text\t${Buffer.from(value).toString('base64')}`);
const output = result => result.events.filter(e => e.type === 'console').map(e => e.text).join('');

test('all window controls, Unicode editing and callbacks work together', async () => {
  const source = `use window as ui
ui.create<"Class club", 600, 900>
ui.label<"Choose your activities">
var name = ui.input<"Name">
var interests = ui.checklist<"Interests", list<"Programming", "Art">>
var level = ui.radio_group<"Level", list<"Beginner", "Explorer">>
var project = ui.listbox<"Project", list<"Game", "Weather">>
var day = ui.combobox<"Day", list<"Monday", "Tuesday">>
var message = ui.label<"Ready">
func Submit <>:
 assert<name.value is "Åse Ørn">
 assert<length<interests.selected> is 2>
 assert<interests.selected.1 is "Programming">
 assert<interests.selected.2 is "Art">
 assert<level.value is "Explorer">
 assert<project.value is "Weather">
 assert<day.value is "Tuesday">
 message.caption = "Welcome, " + name.value
 print<message.caption>
.
ui.button<"Submit", Submit>
ui.run<>
print<"closed">`;
  const result = await run('all-controls', source, (event, send, frames) => {
    if (event.type === 'frame' && frames === 1) {
      text(send, 'wrong'); key(send, 'A', false, true); text(send, 'Ørn'); key(send, 'Home'); text(send, 'Åse ');
      key(send, 'Tab'); key(send, 'Space'); key(send, 'Down'); key(send, 'Space');
      key(send, 'Tab'); key(send, 'Down'); key(send, 'Tab'); key(send, 'Down');
      key(send, 'Tab'); key(send, 'Space'); key(send, 'Down'); key(send, 'Return');
      key(send, 'Tab'); key(send, 'Return');
    }
    if (event.type === 'console' && event.text.includes('Welcome')) key(send, 'Escape');
  });
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  assert.match(output(result), /Welcome, Åse Ørn\nclosed/);
  assert.ok(result.events.some(e => e.type === 'window-mode' && e.enabled));
  assert.ok(result.events.some(e => e.type === 'window-mode' && !e.enabled));
});

test('mouse selections, combo popup and disabled buttons behave correctly', async () => {
  const result = await run('mouse', `use window as ui
ui.create<"Mouse", 480, 650>
var choices = ui.checklist<"Pick", list<"A", "B">>
var radio = ui.radio_group<"Radio", list<"A", "B">>
var combo = ui.combobox<"Combo", list<"A", "B">>
func Bad <>:
 error<"disabled button ran">
.
var disabled = ui.button<"Disabled", Bad>
disabled.enabled = false
func Done <>:
 assert<choices.selected.1 is "B">
 assert<radio.value is "B">
 assert<combo.value is "B">
 print<"mouse passed">
 ui.close<>
.
ui.button<"Done", Done>
ui.run<>`, (event, send, frames) => {
    if (event.type === 'frame' && frames === 1) {
      // Groups begin at 64 and 178; combo begins at 292, then two buttons.
      send('window-click\t80\t144'); send('window-click\t80\t258');
      send('window-click\t80\t332'); send('window-click\t80\t408');
      send('window-click\t80\t392'); send('window-click\t80\t446');
    }
  });
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  assert.match(output(result), /mouse passed/);
});

test('control state and callback closures survive collection; Tab skips disabled controls', async () => {
  const result = await run('closure', `use window as ui
ui.create<"Closure", 480, 300>
func Make <>:
 var count = 41
 ret func <>:
  count++
  assert<count is 42>
  print<count>
  ui.close<>
 .
.
var disabled = ui.input<"Disabled">
disabled.enabled = false
ui.button<"Count", Make<>>
repeat 1500 times:
 var scratch = obj<items = list<1, 2>>
.
ui.run<>`, (event, send, frames) => { if (event.type === 'frame' && frames === 1) key(send, 'Return'); });
  assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
  assert.equal(output(result), '42\n');
});

test('button callback failures report the callback source line', async () => {
  const result = await run('callback-error', 'use window as ui\nui.create<"Error">\nfunc Fail <>:\nerror<"Button failed">\n.\nui.button<"Fail", Fail>\nui.run<>', (event, send, frames) => {
    if (event.type === 'frame' && frames === 1) key(send, 'Return');
  });
  assert.equal(result.code, 1);
  const error = result.events.find(e => e.type === 'error'); assert.equal(error.line, 4); assert.match(error.message, /Button failed/);
});

test('window event loops respond to pause, resume and stop', async () => {
  let paused = false;
  const result = await run('control', 'use window\nwindow.create<"Controls">\nwindow.label<"Waiting">\nwindow.run<>', (event, send, frames) => {
    if (event.type === 'frame' && frames === 1) send('pause');
    if (event.type === 'paused') { paused = true; send('resume'); }
    if (event.type === 'state' && event.state === 'running' && paused) send('stop');
  });
  assert.equal(paused, true); assert.equal(result.events.at(-1).status, 'stopped');
});
test('English and Norwegian window examples run and close cleanly', async () => {
  for (const language of ['english', 'norsk']) for (const example of ['hello', 'club']) {
    const source = await readFile(`examples/windows/${language}/${example}.slang`, 'utf8');
    const result = await run(`${language}-${example}`, source, (event, send, frames) => {
      if (event.type === 'frame' && frames === 1) key(send, 'Escape');
    });
    assert.equal(result.code, 0, JSON.stringify(result.events.filter(e => e.type === 'error')));
    assert.ok(result.frames >= 1);
  }
});
