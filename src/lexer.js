import { keywords } from './keywords.js';

export class SlangSyntaxError extends Error {
  constructor(message, token) {
    super(`${message} (line ${token.line}, column ${token.column})`);
    this.name = 'SlangSyntaxError';
    this.line = token.line;
    this.column = token.column;
  }
}

export function tokenize(source) {
  const tokens = [];
  let offset = 0;
  let line = 1;
  let column = 1;
  const advance = () => {
    const char = source[offset++];
    if (char === '\n') { line++; column = 1; }
    else column++;
    return char;
  };
  const fail = message => { throw new SlangSyntaxError(message, { line, column }); };
  const emit = (kind, value, start, raw = source.slice(start.offset, offset)) =>
    tokens.push({ kind, value, raw, ...start });

  while (offset < source.length) {
    const char = source[offset];
    const start = { offset, line, column };
    if (char === ' ' || char === '\t' || char === '\r' || char === '\uFEFF') {
      advance();
      continue;
    }
    if (char === '\n') { advance(); emit('newline', '\n', start); continue; }
    if (char === '#') {
      const directive = source.slice(offset).split('\n', 1)[0];
      if (line !== 1 || tokens.length || !/^#(?:no|en)[ \t\r]*(?:\/\/.*)?$/.test(directive)) throw new SlangSyntaxError('Language directive must be #no or #en on the first line', start);
      while (offset < source.length && source[offset] !== '\n') advance();
      continue;
    }
    if (source.startsWith('//', offset)) {
      while (offset < source.length && source[offset] !== '\n') advance();
      continue;
    }
    if (char === '"') {
      advance();
      let closed = false;
      while (offset < source.length) {
        if (source[offset] === '\n' || source[offset] === '\r') fail('Text must end on the same line');
        if (source[offset] === '"') { advance(); closed = true; break; }
        if (source[offset] === '\\') {
          advance();
          if (offset >= source.length || source[offset] === '\n' || source[offset] === '\r') {
            fail('Unfinished text escape');
          }
        }
        advance();
      }
      if (!closed) throw new SlangSyntaxError('Unterminated text', start);
      const raw = source.slice(start.offset, offset);
      let value;
      try { value = JSON.parse(raw); }
      catch { throw new SlangSyntaxError('Invalid text escape', start); }
      emit('text', value, start);
      continue;
    }
    if (/[0-9]/u.test(char)) {
      // After member-access '.', digits denote an index, not a decimal literal.
      const isIndex = tokens.at(-1)?.kind === 'symbol' && tokens.at(-1)?.value === '.';
      while (offset < source.length && /[0-9]/u.test(source[offset])) advance();
      let kind = 'int';
      if (!isIndex && source[offset] === '.' && /[0-9]/u.test(source[offset + 1] ?? '')) {
        kind = 'dec';
        advance();
        while (offset < source.length && /[0-9]/u.test(source[offset])) advance();
      }
      // Keep numeric spelling; never round through a JavaScript Number.
      emit(kind, source.slice(start.offset, offset), start);
      continue;
    }
    if (/[\p{L}_]/u.test(char)) {
      advance();
      while (offset < source.length && /[\p{L}\p{M}\p{N}_]/u.test(source[offset])) advance();
      const raw = source.slice(start.offset, offset);
      const canonical = keywords.get(raw);
      emit(canonical ? 'keyword' : 'identifier', canonical ?? raw, start);
      continue;
    }
    const pair = source.slice(offset, offset + 2);
    if (['==', '!='].includes(pair)) {
      fail(pair === '==' ? "Use 'is' (or 'er') to compare values" : "Use 'is not' (or 'er ikke') to compare values");
    }
    if (['++', '--', '<=', '>='].includes(pair)) {
      advance(); advance(); emit('symbol', pair, start); continue;
    }
    if ('<>:.,()=+-*/%'.includes(char)) {
      advance(); emit('symbol', char, start); continue;
    }
    fail(`Unexpected character '${char}'`);
  }
  tokens.push({ kind: 'eof', value: '<eof>', raw: '', offset, line, column });
  return tokens;
}
