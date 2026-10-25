import test from 'node:test';
import assert from 'node:assert/strict';
import { completions, visibleSymbols, publicExports, callSignature, libraries, canonicalLibraries } from '../ide/language.mjs';
import { readFileSync } from 'node:fs';
test('completion respects lexical scopes and Norwegian declarations', () => {
  const source = 'var outer = 1\nfunk Add <tall som heltall>:\n var inner = 2\n retur tall\n.\nprint<outer>\n';
  assert.ok(visibleSymbols(source, 4).some(s => s.label === 'tall'));
  assert.ok(!visibleSymbols(source, 6).some(s => s.label === 'inner'));
  assert.ok(completions(source, 6, 1).some(s => s.label === 'stopp'));
});
test('member completion resolves graphics and object fields', () => {
  const source = 'use graphics as g\nobj Animal:\n name as text\n.\nvar dog = Animal<"Rex">\ndog.';
  assert.deepEqual(completions(source, 6, 5).map(s => s.label), ['name']);
  assert.ok(completions('use graphics as g\ng.', 2, 3).some(s => s.label === 'circle'));
  assert.ok(completions('use numbers\nnumbers.', 2, 9).some(s => s.label === 'round'));
});
test('library completion exposes only explicit public declarations', () => {
  const module = 'lib maths:\n func Hidden <x>:\n ret x\n .\n public func Add <a, b>:\n ret a+b\n .\n.\n';
  assert.deepEqual(publicExports(module).map(s => s.label), ['Add']);
  assert.deepEqual(completions('use "maths.slang" as m\nm.', 2, 3, { m: module }).map(s => s.label), ['Add']);
});
test('completion avoids strings and comments; signatures track nested calls', () => {
  assert.equal(completions('print<"hello', 1, 13).length, 0);
  assert.equal(completions('// print', 1, 9).length, 0);
  const source = 'use graphics as g\ng.circle<1, 2, ';
  assert.equal(callSignature(source, 2, 16).activeParameter, 2);
});

const colorHints = (source, locale = 'no') => {
  const lines = source.split('\n');
  return completions(source, lines.length, lines.at(-1).length + 1, {}, locale).map(s => s.label);
};
test('color completion follows the IDE language for both graphics operations and import aliases', () => {
  const source = 'bruk grafikk som g\ng.fyll<"';
  const no = colorHints(source), en = colorHints(source, 'en'), all = colorHints(source, 'all');
  for (const name of ['hvit', 'lilla', 'gul', 'rød', 'grønn', 'blå', 'grå']) assert.ok(no.includes(name));
  for (const name of ['white', 'purple', 'yellow', 'red', 'green', 'blue', 'grey']) {
    assert.ok(en.includes(name)); assert.ok(!no.includes(name));
  }
  assert.ok(!en.includes('hvit'));
  assert.deepEqual(new Set(all), new Set([...no, ...en]));
  assert.equal(all.filter(n => n === 'cyan').length, 1);
  assert.deepEqual(colorHints('use graphics\ngraphics.background<"g', 'no'), no);
  assert.deepEqual(colorHints('bruk grafikk\ngrafikk.bakgrunn<\n    "'), no);
  assert.deepEqual(colorHints('use graphics as tegning\ntegning.fill<"bl'), no);
});
test('color completion excludes comments, ordinary strings, non-color arguments and shadowed modules', () => {
  for (const code of [
    '// g.fill<"', 'g.fill< // "', 'print<"g.fill<\\"', 'print<"hvit',
    'g.save<"', 'g.key_down<"', 'g.fill<"#', 'g.fill<"hvit", "',
    'thing.g.fill<"', 'func F <g>:\ng.fill<"', 'if true:\nvar g = obj<>\ng.fill<"'
  ]) assert.deepEqual(colorHints('use graphics as g\n' + code), [], code);
  assert.deepEqual(colorHints('use "graphics.slang" as g\ng.fill<"'), []);
  assert.deepEqual(colorHints('g.fill<"'), []);
  assert.deepEqual(colorHints('use graphics as g\ng.fill<"gul"'), []);
});
test('every native library export has completion and a signature through an alias', () => {
  const registry = readFileSync(new URL('../native/src/library.cpp', import.meta.url), 'utf8');
  const entries = [...registry.matchAll(/\{"(\w+)", \{([^}]+)\}\}/g)];
  assert.equal(entries.length, canonicalLibraries.length);
  for (const [, name, exports] of entries) {
    const names = [...exports.matchAll(/"(\w+)"/g)].map(m => m[1]).sort();
    assert.deepEqual(completions(`use ${name} as lib_alias\nlib_alias.`, 2, 11, {}, 'en').map(e => e.label).sort(), names);
    for (const member of names) {
      const line = `lib_alias.${member}<`;
      assert.equal(callSignature(`use ${name} as lib_alias\n${line}`, 2, line.length + 1)?.label, member);
    }
  }
});
test('source libraries with bundled names still use their own exports', () => {
  const code = 'lib strings:\npublic func Greeting <>:\nret "Hei"\n.\n.';
  assert.deepEqual(completions('use "strings.slang" as strings\nstrings.', 2, 9, { strings: code }).map(e => e.label), ['Greeting']);
});
test('imports suggest the bundled libraries and identifiers preserve Nordic spelling', () => {
  assert.deepEqual(completions('use ', 1, 5).map(e => e.label).sort(), Object.keys(libraries).sort());
  assert.ok(visibleSymbols('var øre = 0.01\nvar blåbær = 3\n', 3).some(e => e.label === 'øre'));
});
test('window controls expose their properties through aliased imports', () => {
  const source = 'use window as ui\nvar navn = ui.input<"Navn">\nnavn.';
  assert.deepEqual(completions(source, 3, 6).map(s => s.label), ['caption', 'enabled', 'value']);
  const checklist = 'use window som vindu\nvar valg = vindu.checklist<"Velg", liste<>>\nvalg.';
  assert.deepEqual(completions(checklist, 3, 6).map(s => s.label), ['caption', 'enabled', 'items', 'selected']);
  assert.equal(completions('use "window.slang" as ui\nvar navn = ui.input<"Navn">\nnavn.', 3, 6).length, 0);
});
