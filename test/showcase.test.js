import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { cpSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { parse } from '../src/parser.js';
import { Decoder } from '../ide/protocol.cjs';

const executable = path.resolve(process.env.SLANG_NATIVE_EXE || process.env.SLANG_BINARY || `build/native-ninja/slang${process.platform === 'win32' ? '.exe' : ''}`);
mkdirSync('build/showcase-tests', { recursive: true });
const workspace = mkdtempSync(path.resolve('build/showcase-tests/run-'));
// All file-writing examples run against copies, never the shipped sources.
cpSync('examples/showcase', path.join(workspace, 'showcase'), { recursive: true });
cpSync('examples/graphics', path.join(workspace, 'graphics'), { recursive: true });
const languages = ['english', 'norsk'];
function run(language, name, input = '') {
  const result = spawnSync(executable, ['run', path.join(workspace, 'showcase', language, name)], { input, encoding: 'utf8', timeout: 5000 });
  assert.equal(result.status, 0, result.stderr || String(result.error));
  return result.stdout.replaceAll('\r\n', '\n');
}

test('every showcase has a pair, and native/JavaScript syntax agrees', () => {
  let checked = 0;
  for (const directory of ['showcase', 'graphics']) {
    const files = language => readdirSync(path.join(workspace, directory, language), { recursive: true }).filter(name => name.endsWith('.slang')).sort();
    assert.deepEqual(files('english'), files('norsk'));
    for (const language of languages) for (const name of files(language)) {
      const file = path.join(workspace, directory, language, name);
      const native = spawnSync(executable, ['ast', file], { encoding: 'utf8', timeout: 5000 });
      assert.equal(native.status, 0, native.stderr);
      assert.deepEqual(JSON.parse(native.stdout), parse(readFileSync(file, 'utf8')), file);
      checked++;
    }
  }
  assert.equal(checked, 24); // 22 runnable lessons plus two imported helper modules.
});

test('greetings accept Scandinavian names, blank input and EOF', () => {
  for (const language of languages) {
    assert.equal(run(language, '01-hello.slang', '  Åse Ørn  \n').match(/Åse Ørn/g)?.length, 3);
    const fallback = language === 'english' ? 'friend' : 'venn';
    for (const input of ['', '  \n']) assert.equal(run(language, '01-hello.slang', input).split(fallback).length - 1, 3);
  }
});

test('pocket money retains exact decimals and arbitrary integers in both languages', () => {
  assert.match(run('english', '02-pocket-money.slang'), /Week 4 50\.00 kr\nYou can buy it! Money left: 10\.10 kr\nA large integer: 1267650600228229401496703205376/);
  assert.match(run('norsk', '02-pocket-money.slang'), /Uke 4 50,00 kr\nDu har råd! Penger igjen: 10,10 kr\nEt stort heltall: 1267650600228229401496703205376/);
});

test('weather parses dot and comma data into equivalent measurements', () => {
  const english = run('english', '03-weather.slang'), norwegian = run('norsk', '03-weather.slang');
  for (const output of [english, norwegian]) assert.match(output, /list<12\.4, 12\.6, 12\.5, 12\.3>/);
  assert.match(english, /Lowest: 12\.3\nHighest: 12\.6\nAverage: 12\.45/);
  assert.match(norwegian, /Lavest: 12\.3\nHøyest: 12\.6\nGjennomsnitt: 12,45/);
});

test('pet objects share field updates through function parameters', () => {
  assert.equal(run('english', '04-pets.slang'), 'Feeding Rex\nFeeding Kiwi\nRex is already full.\nFirst pet: Rex Legs: 4\n');
  assert.equal(run('norsk', '04-pets.slang'), 'Gir mat til Rex\nGir mat til Kiwi\nRex er allerede mett.\nFørste dyr: Rex Bein: 4\n');
});

test('closures keep independent state and functions can be passed as values', () => {
  assert.equal(run('english', '05-counters.slang'), 'Red: 1 2\nBlue: 10 20\nDouble twice: 12\n');
  assert.equal(run('norsk', '05-counters.slang'), 'Rød: 1 2\nBlå: 10 20\nDoble to ganger: 12\n');
});

test('relative helper libraries expose types and functions from any working directory', () => {
  assert.equal(run('english', '06-library.slang'), 'Total: 29.90 kr\n');
  assert.equal(run('norsk', '06-library.slang'), 'Sum: 29,90 kr\n');
});

test('notebooks preserve existing entries on a second run', () => {
  for (const language of languages) {
    const first = run(language, '07-notebook.slang');
    const second = run(language, '07-notebook.slang');
    const saved = readFileSync(path.join(workspace, 'showcase', language, 'my-notebook.txt'), 'utf8');
    assert.equal(saved.split('\n').length, 4);
    assert.ok(second.startsWith(first.trimEnd()));
    assert.match(saved, /æ, ø/);
  }
});

test('guessing games reject invalid input, can be won, and handle quit/EOF', () => {
  for (const language of languages) {
    const output = run(language, '08-guess.slang', 'bad\n1\n2\n3\n4\n5\n6\n');
    assert.match(output, language === 'english' ? /Please type a whole number/ : /Skriv et helt tall/);
    assert.match(output, language === 'english' ? /Correct! Attempts: [1-6]/ : /Riktig! Forsøk: [1-6]/);
    for (const input of ['', language === 'english' ? ' QUIT \n' : ' SLUTT \n']) {
      assert.doesNotMatch(run(language, '08-guess.slang', input), /Correct!|Riktig!/);
    }
  }
});

async function graphics(language, lesson, action, snapshot) {
  const file = path.join(workspace, 'graphics', language, lesson);
  return new Promise((resolve, reject) => {
    const child = spawn(executable, ['ide', file, ...(snapshot ? ['--snapshot', snapshot] : [])], {
      windowsHide: true, env: { ...process.env, SDL_VIDEO_DRIVER: 'dummy', SDL_RENDER_DRIVER: 'software' },
    });
    const decoder = new Decoder(); let frames = 0, text = '', stderr = '', lastFrame, failure;
    const send = line => child.stdin.write(line + '\n');
    const timer = setTimeout(() => { child.kill(); reject(new Error(`${language}/${lesson} timed out`)); }, 10000);
    child.stdin.on('error', () => {});
    child.on('error', error => { clearTimeout(timer); reject(error); });
    child.stderr.on('data', bytes => { stderr += bytes; });
    child.stdout.on('data', bytes => { try { decoder.push(bytes); } catch (error) { failure = error; child.kill(); } });
    decoder.on('packet', event => {
      try {
        if (event.type === 'frame') { frames++; lastFrame = event; }
        if (event.type === 'console') text += event.text;
        if (event.type === 'error') throw new Error(event.message);
        action?.(event, send, frames);
        if (event.type === 'ready') send('start');
        if (event.type === 'frame') send('frame-ack');
      } catch (error) { failure = error; child.kill(); }
    });
    child.on('close', code => {
      clearTimeout(timer);
      try { decoder.end(); assert.equal(code, 0, stderr); if (failure) throw failure; resolve({ frames, text, lastFrame }); }
      catch (error) { reject(failure || error); }
    });
  });
}

test('implicit charts render identical pixels in both languages', async () => {
  const results = [];
  for (const language of languages) {
    // Save a real SDL-rendered preview without changing the teaching source.
    const snapshot = path.join(workspace, `chart-${language}.slang`);
    const image = path.join(workspace, `chart-${language}.png`).replaceAll('\\', '/');
    writeFileSync(snapshot, readFileSync(path.join(workspace, 'graphics', language, '01-chart.slang'), 'utf8') + `\ng.save<${JSON.stringify(image)}>\n`);
    results.push(await graphics(language, '01-chart.slang', undefined, snapshot));
  }
  const [a, b] = results.map(result => result.lastFrame);
  assert.equal(a.width, 640); assert.equal(a.height, 480);
  assert.deepEqual(a.pixels, b.pixels);
  const at = (x, y) => [...a.pixels.subarray((y * a.width + x) * 4, (y * a.width + x) * 4 + 4)];
  assert.deepEqual(at(0, 0), [24, 36, 58, 255]);
  assert.deepEqual(at(100, 350), [255, 112, 166, 255]);
  assert.deepEqual(at(200, 350), [112, 214, 255, 255]);
  console.log(`Chart previews: ${workspace}`);
});

test('callback animations freeze, move, and exit on keyboard input', async () => {
  for (const language of languages) {
    let frozen;
    const result = await graphics(language, '02-bounce.slang', (event, send, frames) => {
      if (event.type === 'ready') send('key\tSpace\t1');
      if (event.type !== 'frame') return;
      if (frames === 2) frozen = Buffer.from(event.pixels);
      if (frames === 3) { assert.deepEqual(event.pixels, frozen); send('key\tSpace\t0'); }
      if (frames === 10) { assert.notDeepEqual(event.pixels, frozen); send('key\tEscape\t1'); }
    });
    assert.ok(result.frames >= 10);
    assert.match(result.text, language === 'english' ? /Animation finished/ : /Animasjonen er ferdig/);
  }
});

test('coin games move the sprite, collect a coin, and close with Escape', async () => {
  for (const language of languages) {
    let collected = false;
    const result = await graphics(language, '03-coins.slang', (event, send) => {
      if (event.type === 'ready') send('key\tRight\t1');
      if (event.type === 'console' && /(?:Coins|Mynter): 1 \/ 5/.test(event.text)) {
        collected = true; send('key\tRight\t0'); send('key\tEscape\t1');
      }
    });
    assert.equal(collected, true);
    assert.ok(result.frames > 1);
    assert.match(result.text, language === 'english' ? /Game finished/ : /Spillet er ferdig/);
  }
});
