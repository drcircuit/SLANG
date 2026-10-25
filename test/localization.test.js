import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import aliases from '../src/standard-aliases.json' with { type: 'json' };
import { parse } from '../src/parser.js';
import { completions, callSignature, publicExports } from '../ide/language.mjs';
const binary = process.env.SLANG_BINARY || path.resolve(`build/native-ninja/slang${process.platform === 'win32' ? '.exe' : ''}`);
const folder = path.resolve('build/locale-tests'); mkdirSync(folder,{recursive:true});
function run(source, args=['run']) {
  const file=path.join(folder,'main.slang'); writeFileSync(file,source);
  return spawnSync(binary,[...args,file],{encoding:'utf8',timeout:5000,env:{...process.env,SDL_VIDEO_DRIVER:'dummy',SDL_RENDER_DRIVER:'software'}});
}
test('every standard-library alias is the same callable; import aliases share the module', () => {
  const lines=[];
  for (const [alias,original] of Object.entries(aliases.modules)) {
    lines.push(`bruk ${alias}`, `use ${original}`, `sjekk<${alias} is ${original}>`);
    for (const [member,canonical] of Object.entries(aliases.functions[original])) lines.push(`sjekk<${alias}.${member} is ${original}.${canonical}>`);
  }
  for (const [alias,original] of Object.entries(aliases.builtins)) lines.push(`sjekk<${alias} is ${original}>`);
  lines.push('var verdi som desimal = 0.1','var ok som logisk = sann','var ting som objekt = objekt<navn = "Åse">','sjekk<verdi + 0.2 er 0.3>','skriv<ting.navn>');
  const source=lines.join('\n'); const r=run(source); assert.equal(r.status,0,r.stderr); assert.equal(r.stdout.trim(),'Åse');
  const ast=run(source,['ast']); assert.deepEqual(JSON.parse(ast.stdout),parse(source));
});
test('window property aliases share cells without renaming student object fields', () => {
  const r=run('bruk vindu\nvindu.opprett<"Prøve">\nvar felt = vindu.tekstfelt<"Navn", "Åse">\nsjekk<felt.verdi er felt.value>\nfelt.verdi = "Ørn"\nsjekk<felt.value er "Ørn">\nfelt.tekst = "Fugl"\nsjekk<felt.caption er "Fugl">\nvar valg = vindu.sjekkliste<"Velg", liste<"a", "b">>\nlegg_til<valg.valgte, "b">\nsjekk<valg.selected.1 er "b">\nsjekk<lengde<felt> er 3>\nvindu.lukk<>\nvar eget = objekt<verdi = 1, value = 2>\nsjekk<eget.verdi er 1>\nsjekk<eget.value er 2>');
  assert.equal(r.status,0,r.stderr);
});
test('language-specific completion filters standard names but preserves user symbols', () => {
  for (const [no,en] of Object.entries(aliases.modules)) {
    for (const imported of [no,en]) {
      const src=`bruk ${imported} som verktøy\nverktøy.`;
      assert.deepEqual(completions(src,2,9,{},'no').map(x=>x.label).sort(),Object.keys(aliases.functions[en]).sort());
      assert.deepEqual(completions(src,2,9,{},'en').map(x=>x.label).sort(),[...new Set(Object.values(aliases.functions[en]))].sort());
    }
  }
  const fields='bruk vindu\nvar felt = vindu.tekstfelt<"Navn">\nfelt.';
  assert.deepEqual(completions(fields,3,6,{},'no').map(x=>x.label),['tekst','aktiv','verdi']);
  assert.equal(callSignature('bruk vindu\nvindu.knapp<',2,13)?.label,'knapp');
  const labels=locale=>completions('var custom = 1\n',2,1,{},locale).map(x=>x.label);
  assert.ok(labels('no').includes('custom')); assert.ok(labels('no').includes('hvis')); assert.ok(!labels('no').includes('if'));
  assert.ok(labels('en').includes('if')); assert.ok(!labels('en').includes('hvis'));
  assert.ok(!labels('no').includes('list')); assert.ok(!labels('no').includes('obj'));
  const list=completions('',1,1,{},'no').find(x=>x.label==='liste');
  assert.equal(list.signature,'liste<elementer…>'); assert.match(list.detail,/tom liste/);
  assert.match(callSignature('bruk vindu\nvindu.knapp<',2,13,{},'no').detail,/Legg til en knapp/);
  assert.deepEqual(publicExports('bibliotek m:\noffentlig funk Hei <>:\nretur 1\n.\n.').map(x=>x.label),['Hei']);
});

test('REPL accepts Norwegian and English names in the same persistent session', () => {
  const r=spawnSync(binary,['repl'],{encoding:'utf8',timeout:5000,input:'var øre = 0.01\nskriv<øre>\nprint<øre + 0.02>\nbruk tall\nprint<tall.formater<øre, 2, ",">>\n:quit\n'});
  assert.equal(r.status,0,r.stderr);
  assert.match(r.stdout,/0\.01/); assert.match(r.stdout,/0\.03/); assert.match(r.stdout,/0,01/);
});
test('directives localize syntax/runtime messages without moving source positions', () => {
  for (const locale of ['no','en']) {
    const source=`#${locale}\nvar tall =\n`;
    const r=run(source,['check','--json']); const data=JSON.parse(r.stdout);
    assert.equal(r.status,1); assert.equal(data.diagnostics[0].start.line,2);
    assert.match(data.diagnostics[0].message,locale==='no'?/Forventet/:/Expected/);
  }
  const r=run('#no\nprint<1 / 0>'); assert.match(r.stderr,/Kan ikke dele på null/); assert.match(r.stderr,/:2:/);
  assert.match(run('#en\nprint<1 / 0>',['run','--locale','no']).stderr,/Cannot divide by zero/);
  assert.match(run('print<1 / 0>',['run','--locale','no']).stderr,/Kan ikke dele på null/);
  assert.match(run('#no\nfeil<"Expected a name">').stderr,/Expected a name/);
  assert.match(run('#no\nsjekk<usann, "Keep my English">').stderr,/Sjekken feilet: Keep my English/);
  assert.throws(()=>parse('print<1>\n#no'),/first line/);
  assert.throws(()=>parse('#xx\nprint<1>'),/first line/);
  const valid='#no // norsk\nvar svar = 42\nskriv<svar>\n';
  assert.deepEqual(JSON.parse(run(valid,['ast']).stdout),parse(valid));
});
test('module diagnostics use the directive in the file where the error occurs', () => {
  writeFileSync(path.join(folder,'norsk.slang'),'#no\nbibliotek test:\noffentlig funk Kjør <>:\nskriv<1 / 0>\n.\n.');
  const r=run('#en\nuse "norsk.slang" as test\ntest.Kjør<>'); assert.match(r.stderr,/Kan ikke dele på null/); assert.match(r.stderr,/norsk.slang:4:/);
});
