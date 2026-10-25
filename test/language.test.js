import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { parse as referenceParse } from '../src/parser.js';
import { tokenize } from '../src/lexer.js';
import { keywordGroups, keywords } from '../src/keywords.js';

// Optional port verification. Native builds and native tests do not require Node.
const nativeExecutable = process.env.SLANG_NATIVE_EXE;
const parse = nativeExecutable ? source => {
  const result = spawnSync(nativeExecutable, ['ast', '-'], { input: source, encoding: 'utf8' });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(result.stderr);
  const ast = JSON.parse(result.stdout);
  assert.deepEqual(ast, referenceParse(source), 'native AST differs from the syntax reference');
  return ast;
} : referenceParse;

const value = source => parse(`var result = ${source}`).body[0].value;
const expression = source => parse(source).body[0].expression;

test('library declarations, visibility, imports, and qualified types', () => {
  const ast = parse('use console\nuse "players.slang" as players\nlib maths:\nfunc Validate <val>:\nassert<val >= 0>\n.\npublic func Square <val>:\nValidate<val>\nret val * val\n.\npublic obj Point:\nx as int\n.\n.\nvar p as players.Player = players.Create<"Arne">');
  assert.deepEqual(ast.body[0], { type: 'Import', path: 'console', alias: 'console', bundled: true });
  assert.equal(ast.body[1].bundled, false);
  assert.equal(ast.body[2].type, 'Library');
  assert.equal(ast.body[2].body[0].type, 'FunctionDeclaration');
  assert.equal(ast.body[2].body[1].type, 'Public');
  assert.equal(ast.body[2].body[2].declaration.type, 'ObjectDeclaration');
  assert.equal(ast.body[3].annotation.name, 'players.Player');
  assert.equal(parse('use console as io').body[0].alias, 'io');
  assert.deepEqual(parse('lib m:\npublic func F <x as int>:\nret x\n.\n.'), parse('lib m:\npublic funk F <x som heltall>:\nretur x\n.\n.'));
});

test('public and imports have explicit placement rules', () => {
  for (const source of ['public func F <>:\n.', 'lib m:\npublic var x = 1\n.', 'lib m:\npublic public func F <>:\n.\n.', 'func F <>:\nuse console\n.', 'lib m:\nfunc F <>:\npublic func G <>:\n.\n.\n.', 'lib a:\nlib b:\n.\n.', 'use "maths.slang"']) assert.throws(() => parse(source));
});

test('English and Norwegian example programs have identical ASTs', () => {
  const english = readFileSync(new URL('../examples/english.slang', import.meta.url), 'utf8');
  const norwegian = readFileSync(new URL('../examples/norsk.slang', import.meta.url), 'utf8');
  assert.deepEqual(parse(english), parse(norwegian));
});

test('aliases have no conflicts and normalize every keyword', () => {
  const entries = Object.values(keywordGroups).flat();
  assert.equal(entries.length, keywords.size);
  for (const [canonical, aliases] of Object.entries(keywordGroups)) {
    for (const alias of aliases) assert.equal(tokenize(alias)[0].value, canonical);
  }
});

test('eller resolves branches and logical expressions by context', () => {
  const english = `while true:
if a or b and c:
if d:
print<1>
else:
print<2>
.
else if d or e:
print<3>
else:
break
.
.
`;
  const norwegian = english.replace(/\b(while|true|if|or|and|else|break)\b/g,
    word => ({ while: 'mens', true: 'sann', if: 'hvis', or: 'eller', and: 'og', else: 'eller', break: 'stopp' })[word]);
  const ast = parse(norwegian);
  assert.deepEqual(ast, parse(english));
  const outer = ast.body[0].body[0];
  assert.equal(outer.branches[0].condition.operator, 'or');
  assert.equal(outer.branches[0].condition.right.operator, 'and');
  assert.equal(outer.branches[0].body[0].otherwise[0].type, 'ExpressionStatement');
  assert.equal(outer.otherwise[0].type, 'Break');
  assert.deepEqual(value('check<a eller b>'), value('check<a or b>'));
  assert.deepEqual(value('(a\neller b)'), value('(a\nor b)'));
  assert.equal(value('"eller stopp"').value, 'eller stopp');
});

test('contextual Norwegian keywords preserve distinct English keywords', () => {
  assert.equal(tokenize('stopp')[0].value, 'break');
  assert.equal(tokenize('ellers')[0].kind, 'identifier');
  assert.equal(tokenize('bryt')[0].kind, 'identifier');
  for (const source of ['var x = a else b', 'if a:\nprint<1>\nor:\nprint<2>\n.', 'eller:\nprint<1>\n.']) {
    assert.throws(() => parse(source));
  }
});

test('localization preserves strings and Unicode identifiers', () => {
  const ast = parse('var blåbær = "hvis func sann" // ellers\nvar i = 1');
  assert.equal(ast.body[0].name, 'blåbær');
  assert.equal(ast.body[0].value.value, 'hvis func sann');
  assert.equal(ast.body[1].name, 'i');
  assert.equal(value('"if"').value, 'if');
  assert.equal(expression('"while"').value, 'while');
  assert.throws(() => parse('var x = 1 "is" 1'));
});

test('indentation has no effect on the program', () => {
  const source = 'while active:\n    if ready:\n        count++\n    .\n.\n';
  assert.deepEqual(parse(source), parse(source.replace(/^ +/gm, '')));
  assert.deepEqual(parse(source), parse(source.replace(/    /g, '\t')));
});

test('integer and decimal literals retain all source digits', () => {
  const integer = '12345678901234567890123456789012345678901234567890';
  const decimal = '12345678901234567890.123456789012345678900';
  assert.deepEqual(value(integer), { type: 'Literal', kind: 'int', value: integer });
  assert.deepEqual(value(decimal), { type: 'Literal', kind: 'dec', value: decimal });
});

test('nested calls and comparisons in angle arguments', () => {
  assert.equal(expression('Add<2, Multiply<3, 4>>').arguments[1].value.callee.name, 'Multiply');
  for (const operator of ['>', '>=', '<=', 'is', 'is not']) {
    assert.equal(expression(`check<a ${operator} b>`).arguments[0].value.operator, operator);
  }
  assert.equal(expression('check<(a < b)>').arguments[0].value.value.operator, '<');
  const combined = expression('check<Add<a, b> > Multiply<c, d>>');
  assert.equal(combined.arguments[0].value.operator, '>');
  assert.equal(expression('Add<2, 3> > 4').operator, '>');
  assert.equal(expression('check<a >= b and c is not none>').arguments[0].value.operator, 'and');
});

test('ambiguous angle expressions fail instead of silently choosing behavior', () => {
  // Either check(a < b), or check < a(b): both are syntactically possible.
  assert.throws(() => parse('check<a < b>'), /Ambiguous angle brackets/);
  assert.throws(() => parse('f<a > +b> + c'), /Ambiguous angle brackets/);
  assert.equal(expression('f<(a > +b)> + c').operator, '+');
  assert.equal(expression('(f<a> + b) > +c').operator, '>');
});

test('complete call statements stop before following calls and function declarations', () => {
  const ast = parse('print<round<2.345, 2>>\nprint<round<2.355, 2>>\nfunc Next <>:\nret 1\n.\nprint<Next<>>');
  assert.deepEqual(ast.body.map(node => node.type), ['ExpressionStatement', 'ExpressionStatement', 'FunctionDeclaration', 'ExpressionStatement']);
});

test('newline continuation works inside calls and parentheses', () => {
  assert.equal(value('Add<\n  2,\n  Multiply<3, 4>\n>').arguments.length, 2);
  assert.equal(value('(1 +\n  2)').value.operator, '+');
  assert.throws(() => parse('var x = 1 +\n2'), /complete expression/);
});

test('commas create lists in initializers and separate function arguments', () => {
  assert.equal(value('1, 2, 3').type, 'ListLiteral');
  assert.equal(value('1').kind, 'int');
  assert.equal(value('list<>').arguments.length, 0);
  assert.equal(value('list<1>').arguments.length, 1);
  assert.equal(expression('process<list<1, 2, 3>>').arguments.length, 1);
});

test('object definitions, named fields, and positional anonymous objects', () => {
  const ast = parse('obj Animal:\nlegs as int\nfurry as bool\nname as text\n.\nvar dog = Animal<4, true, "Rex">');
  assert.deepEqual(ast.body[0].fields.map(field => field.name), ['legs', 'furry', 'name']);
  assert.deepEqual(value('obj<age = 1, legs = 2, name = "Arne">').arguments.map(arg => arg.name), ['age', 'legs', 'name']);
  assert.deepEqual(value('obj<1, 2, "Arne">').arguments.map(arg => arg.name), [null, null, null]);
  assert.throws(() => value('obj<1, name = "Arne">'), /complete expression/);
});

test('fixed and computed indexing coexist with decimal literals', () => {
  assert.equal(value('a.1').index.value, '1');
  assert.equal(value('a.1.2').object.index.value, '1');
  assert.equal(value('a.<i + 1>').index.operator, '+');
  assert.equal(value('dog.name').name, 'name');
  assert.equal(value('1.25').kind, 'dec');
  assert.throws(() => value('a.0'), /Indices start at 1/);
  assert.equal(parse('a.<i> = 99').body[0].target.type, 'Index');
});

test('prefix and postfix updates preserve their order in the AST', () => {
  for (const operator of ['++', '--']) {
    assert.equal(value(`${operator}a`).prefix, true);
    assert.equal(value(`a${operator}`).prefix, false);
  }
  const result = value('a++ + ++a');
  assert.equal(result.left.prefix, false);
  assert.equal(result.right.prefix, true);
  assert.throws(() => value('1++'), /complete expression/);
});

test('arithmetic, boolean operations, and comparisons respect precedence', () => {
  assert.equal(value('2 + 3 * 4').right.operator, '*');
  assert.equal(value('a or b and c').right.operator, 'and');
  assert.equal(value('not a is b').value.operator, 'is');
  assert.throws(() => value('a < b < c'), /complete expression/);
});

test('typed functions and explicit conversions', () => {
  const fn = parse('func Add <a as int, b as text>:\nret text(a) + b\n.').body[0];
  assert.equal(fn.parameters[0].annotation.name, 'int');
  assert.equal(fn.body[0].value.left.type, 'Conversion');
  assert.equal(fn.body[0].value.left.target, 'text');
  assert.throws(() => parse('print("Hello")'), /complete expression|end of the line/);
});

test('closures and anonymous function declarations parse', () => {
  const outer = parse('func MakeCounter <>:\nvar count = 0\nfunc Next <>:\ncount++\nret count\n.\nret Next\n.').body[0];
  assert.equal(outer.body[1].type, 'FunctionDeclaration');
  assert.equal(outer.body[2].value.name, 'Next');
  assert.equal(value('func <x>:\nret x * 2\n.').type, 'FunctionExpression');
});

test('loops and one period for a full conditional chain', () => {
  const ast = parse('for i from 5 to 1 step -1:\ncontinue\n.\nfor v in 1, 2, 3:\nbreak\n.\nrepeat 3 times:\nprint<1>\n.\nif a:\nret 1\nelse if b:\nret 2\nelse:\nret 3\n.');
  assert.deepEqual(ast.body.map(node => node.type), ['ForRange', 'ForEach', 'Repeat', 'If']);
  assert.equal(ast.body[0].step.value.value, '1');
  assert.equal(ast.body[1].values.type, 'ListLiteral');
  assert.equal(ast.body[3].branches.length, 2);
  assert.equal(ast.body[3].otherwise.length, 1);
});

test('useful errors for mistaken equality and missing terminators', () => {
  assert.throws(() => parse('if x = 1:\nret x\n.'), /Use 'is'/);
  assert.throws(() => parse('x == 1'), /Use 'is'/);
  assert.throws(() => parse('x != 1'), /Use 'is not'/);
  assert.throws(() => parse('while active:\ncount++'), /opened on line 1 needs a closing period/);
  assert.throws(() => parse('while active:\ncount++ .'), /complete expression|end of the line/);
  assert.throws(() => parse('var x = "unfinished'), /Unterminated text/);
});

test('many drawing statements do not become speculative nested calls', () => {
  assert.equal(parse('g.circle<10, 20, 5>\n'.repeat(200)).body.length, 200);
  assert.equal(parse('print<(1 >\n0)>\nprint<2>\n').body.length, 2);
});

test('comments, CRLF, and trailing comments on terminators', () => {
  assert.equal(parse('hvis sann:\r\nprint<"https://example.test">\r\n. // done\r\n').body[0].type, 'If');
});

test('CLI distinguishes parsing from execution and rejects invalid usage', () => {
  const executable = nativeExecutable ?? process.execPath;
  const prefix = nativeExecutable ? [] : ['src/cli.js'];
  const checked = spawnSync(executable, [...prefix, 'check', 'examples/english.slang'], { encoding: 'utf8' });
  assert.equal(checked.status, 0, checked.stderr);
  assert.match(checked.stdout, /not type-checked or executed/);
  const usage = spawnSync(executable, [...prefix, 'nonexistent', 'examples/english.slang'], { encoding: 'utf8' });
  assert.equal(usage.status, 2);
  const missing = spawnSync(executable, [...prefix, 'check', 'examples/missing.slang'], { encoding: 'utf8' });
  assert.equal(missing.status, 1);
});
