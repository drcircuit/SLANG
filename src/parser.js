import { tokenize, SlangSyntaxError } from './lexer.js';

const typeNames = new Set(['int', 'dec', 'text', 'bool', 'list', 'obj']);
const comparisons = new Set(['is', 'is not', '<', '>', '<=', '>=']);
const precedence = { or: 10, and: 20, is: 30, 'is not': 30, '<': 30, '>': 30, '<=': 30, '>=': 30, '+': 40, '-': 40, '*': 50, '/': 50, '%': 50 };
const assignable = node => ['Name', 'Member', 'Index'].includes(node.type);
const bindingPower = node => {
  if (node.type === 'Binary') return precedence[node.operator];
  if (node.type === 'Unary') return node.operator === 'not' ? 25 : 70;
  if (node.type === 'Update') return node.prefix ? 70 : 80;
  return 100;
};

export function parse(source) {
  return new Parser(tokenize(source)).program();
}

class Parser {
  constructor(tokens, position = 0, limit = tokens.length) {
    this.tokens = tokens;
    this.limit = limit;
    this.pos = position;
    this.cache = new Map();
    this.work = 0;
  }

  token(pos = this.pos) { return pos >= this.limit ? this.tokens.at(-1) : (this.tokens[pos] ?? this.tokens.at(-1)); }
  is(value, pos = this.pos) {
    const token = this.token(pos);
    return ['symbol', 'keyword', 'newline', 'eof'].includes(token.kind) && token.value === value;
  }
  isElse() { return this.is('else') || this.is('else_or'); }
  fail(message, pos = this.pos) { throw new SlangSyntaxError(message, this.token(pos)); }
  expect(value) {
    if (!this.is(value)) this.fail(`Expected '${value}', found '${this.token().raw || this.token().value}'`);
    return this.tokens[this.pos++];
  }
  name() {
    if (this.token().kind !== 'identifier') this.fail('Expected a name');
    return this.tokens[this.pos++].value;
  }
  skipLines() { while (this.token().kind === 'newline') this.pos++; }
  skip(pos, multiline) {
    while (multiline && this.token(pos).kind === 'newline') pos++;
    return pos;
  }
  boundary(pos) { return ['newline', 'eof'].includes(this.token(pos).kind); }
  endStatement() {
    if (!this.boundary(this.pos)) this.fail('Expected the end of the line');
    this.skipLines();
  }
  openBlock() {
    this.expect(':');
    if (this.token().kind !== 'newline') this.fail("Put the block's instructions on the next line");
    this.skipLines();
  }
  closeBlock(opening) {
    if (!this.is('.')) this.fail(`The block opened on line ${opening.line} needs a closing period`);
    if (this.token(this.pos - 1).kind !== 'newline') this.fail('A closing period must be on its own line');
    this.pos++;
    if (!this.boundary(this.pos)) this.fail('A closing period must be on its own line');
  }
  program() {
    this.skipLines();
    const body = this.body(false, 'program');
    if (this.token().kind !== 'eof') this.fail('Unexpected block ending');
    return { type: 'Program', body };
  }
  body(stopAtElse = false, context = 'block') {
    const body = [];
    this.skipLines();
    while (this.token().kind !== 'eof' && !this.is('.') && !(stopAtElse && this.isElse())) {
      body.push(this.statement(context));
      this.endStatement();
    }
    return body;
  }
  parseType() {
    if (this.token().kind !== 'identifier' && !(this.token().kind === 'keyword' && typeNames.has(this.token().value))) this.fail('Expected a type');
    let name = this.tokens[this.pos++].value;
    if (!typeNames.has(name)) while (this.is('.')) { this.pos++; name += '.' + this.name(); }
    let element = null;
    if (name === 'list' && this.is('of')) { this.pos++; element = this.parseType(); }
    return { type: 'TypeName', name, element };
  }
  functionDefinition(named) {
    const opening = this.expect('func');
    const name = named ? this.name() : null;
    this.expect('<');
    const parameters = [];
    this.skipLines();
    if (!this.is('>')) {
      do {
        this.skipLines();
        const parameter = { name: this.name(), annotation: null };
        if (this.is('as')) { this.pos++; parameter.annotation = this.parseType(); }
        parameters.push(parameter);
        this.skipLines();
        if (!this.is(',')) break;
        this.pos++;
      } while (true);
    }
    this.expect('>');
    this.openBlock();
    const body = this.body();
    this.closeBlock(opening);
    return { type: named ? 'FunctionDeclaration' : 'FunctionExpression', name, parameters, body };
  }
  statement(context = 'block') {
    // Limits apply independently to each statement's expression search.
    this.work = 0;
    const opening = this.token();
    if (this.is('lib')) {
      if (context !== 'program') this.fail('lib must be at the top level of a file or console submission');
      this.pos++;
      const name = this.name();
      this.openBlock();
      const body = this.body(false, 'library');
      this.closeBlock(opening);
      return { type: 'Library', name, body };
    }
    if (this.is('public')) {
      if (context !== 'library') this.fail('public is only allowed directly inside lib');
      this.pos++;
      if (!(this.is('func') || this.is('obj')) || this.token(this.pos + 1).kind !== 'identifier') this.fail('public must precede a named func or obj declaration');
      return { type: 'Public', declaration: this.statement() };
    }
    if (this.is('use')) {
      if (context === 'block') this.fail('use must be at the top level or directly inside lib');
      this.pos++;
      const bundled = this.token().kind !== 'text';
      let path, alias;
      if (bundled) { path = this.name(); alias = path; }
      else { path = this.tokens[this.pos++].value; this.expect('as'); alias = this.name(); }
      if (bundled && this.is('as')) { this.pos++; alias = this.name(); }
      return { type: 'Import', path, alias, bundled };
    }
    if (this.is('var')) {
      this.pos++;
      const name = this.name();
      let annotation = null;
      if (this.is('as')) { this.pos++; annotation = this.parseType(); }
      this.expect('=');
      const value = this.expressionUntil(p => this.boundary(p), true);
      return { type: 'VariableDeclaration', name, annotation, value };
    }
    if (this.is('func')) return this.functionDefinition(true);
    if (this.is('obj') && this.token(this.pos + 1).kind === 'identifier') {
      this.pos++;
      const name = this.name();
      this.openBlock();
      const fields = [];
      while (!this.is('.') && this.token().kind !== 'eof') {
        const fieldName = this.name();
        this.expect('as');
        fields.push({ name: fieldName, annotation: this.parseType() });
        this.endStatement();
      }
      this.closeBlock(opening);
      return { type: 'ObjectDeclaration', name, fields };
    }
    if (this.is('if')) {
      this.pos++;
      const branches = [];
      let otherwise = null;
      while (true) {
        const condition = this.condition();
        this.openBlock();
        branches.push({ condition, body: this.body(true) });
        if (!this.isElse()) break;
        this.pos++;
        if (this.is('if')) { this.pos++; continue; }
        this.openBlock();
        otherwise = this.body();
        break;
      }
      this.closeBlock(opening);
      return { type: 'If', branches, otherwise };
    }
    if (this.is('while')) {
      this.pos++;
      const condition = this.condition();
      this.openBlock();
      const body = this.body();
      this.closeBlock(opening);
      return { type: 'While', condition, body };
    }
    if (this.is('repeat')) {
      this.pos++;
      const count = this.expressionUntil(p => this.is('times', p));
      this.expect('times');
      this.openBlock();
      const body = this.body();
      this.closeBlock(opening);
      return { type: 'Repeat', count, body };
    }
    if (this.is('for')) {
      this.pos++;
      const variable = this.name();
      let loop;
      if (this.is('in')) {
        this.pos++;
        loop = { type: 'ForEach', variable, values: this.expressionUntil(p => this.is(':', p), true) };
      } else {
        this.expect('from');
        const from = this.expressionUntil(p => this.is('to', p));
        this.expect('to');
        const to = this.expressionUntil(p => this.is('step', p) || this.is(':', p));
        let step = null;
        if (this.is('step')) { this.pos++; step = this.expressionUntil(p => this.is(':', p)); }
        loop = { type: 'ForRange', variable, from, to, step };
      }
      this.openBlock();
      loop.body = this.body();
      this.closeBlock(opening);
      return loop;
    }
    if (this.is('ret')) {
      this.pos++;
      return { type: 'Return', value: this.boundary(this.pos) ? null : this.expressionUntil(p => this.boundary(p), true) };
    }
    if (this.is('break') || this.is('continue')) {
      return { type: this.tokens[this.pos++].value === 'break' ? 'Break' : 'Continue' };
    }
    const left = this.expressionUntil(p => this.is('=', p) || this.boundary(p));
    if (this.is('=')) {
      if (!assignable(left)) this.fail('Assignment needs a variable, field, or list item');
      this.pos++;
      return { type: 'Assignment', target: left, value: this.expressionUntil(p => this.boundary(p), true) };
    }
    return { type: 'ExpressionStatement', expression: left };
  }
  condition() {
    const condition = this.expressionUntil(p => this.is(':', p) || this.is('=', p));
    if (this.is('=')) this.fail("Use 'is' (or 'er') to compare values; '=' assigns a value");
    return condition;
  }
  expressionUntil(ends, sequence = false) {
    const previousLimit = this.limit;
    let lineEnd = this.pos;
    while (lineEnd < previousLimit && !['newline', 'eof'].includes(this.tokens[lineEnd].kind)) lineEnd++;
    if (lineEnd + 1 < previousLimit) {
      this.limit = lineEnd + 1;
      try {
        return this.expressionCandidatesUntil(ends, sequence);
      } catch (error) {
        if (!(error instanceof SlangSyntaxError) || error.alternatives) throw error;
      } finally { this.limit = previousLimit; }
    }
    return this.expressionCandidatesUntil(ends, sequence);
  }
  expressionCandidatesUntil(ends, sequence = false) {
    const start = this.pos;
    const candidates = sequence ? this.sequences(start, false) : this.expressions(start, 0, false);
    let matches = this.unique(candidates.filter(candidate => {
      if (!ends(candidate.pos)) return false;
      // A candidate cannot finish a statement before a bare closing delimiter
      // on the next line. That delimiter belongs to a multiline expression.
      if (this.token(candidate.pos).kind === 'newline') {
        let following = candidate.pos;
        while (this.tokens[following]?.kind === 'newline') following++;
        if (this.tokens[following]?.kind === 'symbol' && ['>', ')', ','].includes(this.tokens[following].value)) return false;
      }
      return true;
    }));
    if (matches.length === 0) this.fail('Expected a complete expression', start);
    // Stop at the first complete statement boundary, including after a call.
    const earliest = Math.min(...matches.map(candidate => candidate.pos));
    matches = matches.filter(candidate => candidate.pos === earliest);
    if (matches.length > 1) {
      const error = new SlangSyntaxError('Ambiguous angle brackets: use parentheses to group the intended expression', this.token(start));
      error.alternatives = matches.map(match => match.node);
      throw error;
    }
    this.pos = matches[0].pos;
    return matches[0].node;
  }
  tick(pos) {
    if (++this.work > 50000) this.fail('Expression is too complex for this prototype; split it into smaller expressions', pos);
  }
  unique(candidates) {
    const seen = new Set();
    return candidates.filter(candidate => {
      const key = JSON.stringify(candidate);
      if (seen.has(key)) return false;
      seen.add(key);
      return true;
    });
  }
  sequences(pos, multiline) {
    const results = [];
    const visit = (items, at) => {
      this.tick(at);
      for (const candidate of this.expressions(at, 0, multiline)) {
        const all = [...items, candidate.node];
        results.push({ node: all.length === 1 ? all[0] : { type: 'ListLiteral', items: all }, pos: candidate.pos });
        const next = this.skip(candidate.pos, multiline);
        if (this.is(',', next)) visit(all, next + 1);
      }
    };
    visit([], pos);
    return this.unique(results);
  }
  expressions(pos, min, multiline) {
    pos = this.skip(pos, multiline);
    const key = `${pos}/${min}/${multiline}/${this.limit}`;
    if (this.cache.has(key)) return this.cache.get(key);
    this.tick(pos);
    const results = [];
    for (const candidate of this.prefixes(pos, multiline)) {
      results.push(...this.grow(candidate.node, candidate.pos, min, multiline));
    }
    const unique = this.unique(results);
    this.cache.set(key, unique);
    return unique;
  }
  prefixes(pos, multiline) {
    this.tick(pos);
    const token = this.token(pos);
    if (['int', 'dec', 'text'].includes(token.kind)) {
      return [{ node: { type: 'Literal', kind: token.kind, value: token.value }, pos: pos + 1 }];
    }
    if (['true', 'false', 'none'].includes(token.value) && token.kind === 'keyword') {
      return [{ node: { type: 'Literal', kind: token.value === 'none' ? 'none' : 'bool', value: token.value === 'none' ? null : token.value === 'true' }, pos: pos + 1 }];
    }
    if (token.value === 'func' && token.kind === 'keyword') {
      if (!this.is('<', pos + 1)) return [];
      const nested = new Parser(this.tokens, pos, this.limit);
      const node = nested.functionDefinition(false);
      return [{ node, pos: nested.pos }];
    }
    if (token.kind === 'identifier' || (token.kind === 'keyword' && typeNames.has(token.value))) {
      if (typeNames.has(token.value) && this.is('(', pos + 1)) {
        return this.expressions(pos + 2, 0, true).flatMap(candidate => {
          const end = this.skip(candidate.pos, true);
          return this.is(')', end) ? [{ node: { type: 'Conversion', target: token.value, value: candidate.node }, pos: end + 1 }] : [];
        });
      }
      return [{ node: { type: 'Name', name: token.value }, pos: pos + 1 }];
    }
    if (token.value === '(') {
      return this.expressions(pos + 1, 0, true).flatMap(candidate => {
        const end = this.skip(candidate.pos, true);
        return this.is(')', end) ? [{ node: { type: 'Group', value: candidate.node }, pos: end + 1 }] : [];
      });
    }
    if (['+', '-', 'not', '++', '--'].includes(token.value)) {
      // 'not' includes comparisons, but stops before 'and'/'or'.
      return this.expressions(pos + 1, token.value === 'not' ? 25 : 70, multiline).flatMap(candidate => {
        if (['++', '--'].includes(token.value) && !assignable(candidate.node)) return [];
        return [{ node: { type: ['++', '--'].includes(token.value) ? 'Update' : 'Unary', operator: token.value, prefix: true, value: candidate.node }, pos: candidate.pos }];
      });
    }
    return [];
  }
  arguments(pos, namedAllowed) {
    const results = [];
    const first = this.skip(pos, true);
    if (this.is('>', first)) results.push({ arguments: [], pos: first + 1 });
    const visit = (args, at, mode = null) => {
      this.tick(at);
      at = this.skip(at, true);
      const named = namedAllowed && this.token(at).kind === 'identifier' && this.is('=', at + 1);
      if (mode !== null && mode !== named) return;
      const name = named ? this.token(at).value : null;
      const start = named ? at + 2 : at;
      for (const candidate of this.expressions(start, 0, true)) {
        const end = this.skip(candidate.pos, true);
        const all = [...args, { name, value: candidate.node }];
        if (this.is('>', end)) results.push({ arguments: all, pos: end + 1 });
        if (this.is(',', end)) visit(all, end + 1, named);
      }
    };
    visit([], first);
    return results;
  }
  grow(node, pos, min, multiline) {
    this.tick(pos);
    const results = [{ node, pos }];
    const next = this.skip(pos, multiline);
    const token = this.token(next);
    const extend = (value, end) => results.push(...this.grow(value, end, min, multiline));
    if (this.is('<', next) && 90 >= min && ['Name', 'Member', 'Index', 'Call', 'Group', 'FunctionExpression'].includes(node.type)) {
      for (const call of this.arguments(next + 1, node.type === 'Name' && node.name === 'obj')) {
        extend({ type: 'Call', callee: node, arguments: call.arguments }, call.pos);
      }
    }
    if (this.is('.', next) && 90 >= min && bindingPower(node) >= 90) {
      const member = this.token(next + 1);
      if (member.kind === 'identifier' || member.kind === 'keyword') extend({ type: 'Member', object: node, name: member.raw }, next + 2);
      if (member.kind === 'int') {
        if (BigInt(member.value) < 1n) this.fail('Indices start at 1', next + 1);
        extend({ type: 'Index', object: node, index: { type: 'Literal', kind: 'int', value: member.value }, computed: false }, next + 2);
      }
      if (this.is('<', next + 1)) {
        for (const index of this.expressions(next + 2, 0, true)) {
          const end = this.skip(index.pos, true);
          if (this.is('>', end)) extend({ type: 'Index', object: node, index: index.node, computed: true }, end + 1);
        }
      }
    }
    if (token.kind === 'symbol' && ['++', '--'].includes(token.value) && 80 >= min && assignable(node)) {
      extend({ type: 'Update', operator: token.value, prefix: false, value: node }, next + 1);
    }
    let operator = ['keyword', 'symbol'].includes(token.kind) ? token.value : undefined;
    if (operator === 'else_or') operator = 'or';
    let rightStart = next + 1;
    if (operator === 'is' && this.is('not', this.skip(rightStart, multiline))) {
      operator = 'is not';
      rightStart = this.skip(rightStart, multiline) + 1;
    }
    const rank = precedence[operator];
    if (rank !== undefined && rank >= min && bindingPower(node) >= rank) {
      // Require 'and' between comparisons unless grouping is explicit.
      if (!(comparisons.has(operator) && node.type === 'Binary' && comparisons.has(node.operator))) {
        for (const right of this.expressions(rightStart, rank + 1, multiline)) {
          extend({ type: 'Binary', operator, left: node, right: right.node }, right.pos);
        }
      }
    }
    return results;
  }
}
