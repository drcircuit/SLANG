import { keywordGroups } from '../src/keywords.js';
import aliases from '../src/standard-aliases.json' with { type: 'json' };
import helpNo from './library-help-no.mjs';
export { keywordGroups };
const entry = (label, parameters, detail) => ({ label, kind: 'function', parameters: parameters ? parameters.split(/,\s*/) : [], detail, signature: `${label}<${parameters}>` });
export const builtins = [
  entry('print', 'value', 'Write a value to the console.'), entry('input', 'prompt?', 'Read one line of text from the console.'),
  entry('assert', 'condition, message?', 'Stop with an error if the condition is false.'), entry('error', 'message', 'Stop this program with an error message.'),
  entry('length', 'value', 'Number of items in a list or characters in text.'), entry('type', 'value', 'Get the type name.'),
  entry('append', 'list, value', 'Add an item to a mutable list.'), entry('remove', 'list, index', 'Remove an item at a one-based index.'),
  entry('round', 'value, places', 'Round a decimal value.'), entry('divide', 'a, b, places', 'Divide with an explicit decimal precision.'),
  entry('list', 'items…', 'Create a list. list<> creates an empty list.'), entry('obj', 'fields…', 'Create an anonymous object.'),
];
export const graphics = [
  entry('canvas', 'width, height, title?', 'Create or resize the canvas. Drawing creates a 640 × 480 canvas automatically.'),
  entry('open', '', 'Process input and begin a frame. Use as the condition of a while loop.'),
  entry('present', '', 'Display the frame and wait for the target frame rate.'), entry('seconds', '', 'Decimal seconds since the previous frame.'),
  entry('run', 'callback', 'Call callback<seconds> for each frame. Input and presentation are automatic.'), entry('close', '', 'Close the canvas and finish its loop.'),
  entry('fps', 'rate', 'Set the target frame rate (default 60).'), entry('key_down', 'key', 'Whether a key is held, for example "right" or "escape".'),
  entry('background', 'color', 'Clear the canvas with a color such as "#18243A".'), entry('fill', 'color', 'Set the drawing color.'),
  entry('line', 'x1, y1, x2, y2', 'Draw a line.'), entry('rect', 'x, y, width, height', 'Draw a filled rectangle.'),
  entry('circle', 'x, y, radius', 'Draw a filled circle.'), entry('sprite', 'path, x, y, width?, height?', 'Draw a PNG or BMP image, relative to this source file.'),
  entry('save', 'path', 'Save the canvas as a PNG image.'),
];
export const libraries = {
  console: builtins.filter(b => ['print', 'input'].includes(b.label)),
  graphics,
  window: [
    entry('create', 'title, width?, height?', 'Create a form with automatic vertical layout. Default size: 480 × 640.'),
    entry('label', 'caption', 'Add a label; change its .caption property to update it.'),
    entry('input', 'label, initial?', 'Add a single-line text input; read/write .value.'),
    entry('button', 'text, callback', 'Add a button. callback<> runs when clicked or activated with Enter/Space.'),
    entry('checklist', 'label, items', 'Add multiple checkboxes; .selected is a list of selected text items.'),
    entry('radio_group', 'label, items', 'Choose one item; read/write .value.'),
    entry('listbox', 'label, items', 'Scrollable single-selection list; read/write .value.'),
    entry('combobox', 'label, items', 'Compact drop-down selector; read/write .value.'),
    entry('run', '', 'Process controls and callbacks until the window closes. Add controls before calling run.'),
    entry('close', '', 'Close the current form. Also usable directly as a button callback.'),
  ],
  numbers: [
    ...builtins.filter(b => ['round', 'divide'].includes(b.label)),
    entry('abs', 'value', 'Absolute value, preserving int or dec.'),
    entry('min', 'a, b', 'The smaller of two numbers.'), entry('max', 'a, b', 'The larger of two numbers.'),
    entry('clamp', 'value, minimum, maximum', 'Keep a number within inclusive bounds.'),
    entry('floor', 'value', 'Round down to an int.'), entry('ceil', 'value', 'Round up to an int.'),
    entry('pow', 'base, exponent', 'Exact power; exponent is an int from 0 to 10000.'),
    entry('sqrt', 'value, places?', 'Decimal square root, ties to even; default 28 fractional places.'),
    entry('sum', 'items', 'Exact sum of a list of numbers; an empty list totals 0.'),
    entry('mean', 'items', 'Arithmetic mean of a non-empty numeric list, rounded to 28 fractional places.'),
    entry('parse', 'value, separator?', 'Parse decimal text; separator is "." (default) or ",". Surrounding whitespace is allowed; no grouping separators.'),
    entry('format', 'value, places, separator?', 'Fixed decimal places with ties to even. Use "," for Norwegian display; default ".".'),
  ],
  strings: [
    entry('length', 'value', 'Count Unicode scalar values, including Scandinavian letters.'),
    entry('contains', 'value, search', 'Whether text contains the exact search text.'),
    entry('find', 'value, search', 'One-based position of the first match, or none.'),
    entry('slice', 'value, start, count?', 'Copy characters from a one-based start; omitted count means the rest.'),
    entry('replace', 'value, search, replacement', 'Replace all non-overlapping matches; search cannot be empty.'),
    entry('split', 'value, separator', 'Split into a list of text, retaining empty parts. Separator cannot be empty.'),
    entry('join', 'items, separator', 'Join a list of text with a separator. Convert numbers explicitly first.'),
    entry('trim', 'value', 'Remove Unicode whitespace from both ends.'),
    entry('lower', 'value', 'Lowercase basic Latin and Latin-1 letters, including ÆØÅÄÖ.'),
    entry('upper', 'value', 'Uppercase basic Latin and Latin-1 letters, including æøåäö.'),
  ],
  lists: [
    ...builtins.filter(b => ['length', 'append', 'remove'].includes(b.label)),
    entry('contains', 'items, value', 'Check membership using the same equality as is.'),
    entry('find', 'items, value', 'One-based position of the first match, or none.'),
    entry('copy', 'items', 'New shallow copy; nested objects and lists remain shared.'),
    entry('slice', 'items, start, count?', 'New shallow slice, using a one-based start and optional count.'),
    entry('sort', 'items', 'Return a sorted copy of all numbers or all text. Text uses Unicode order, not dictionary order.'),
    entry('reverse', 'items', 'Return a reversed shallow copy.'),
  ],
  random: [
    entry('seed', 'value', 'Set an int seed for repeatable exercises in this interpreter session.'),
    entry('integer', 'minimum, maximum', 'Uniform random int between inclusive bounds, including large integers.'),
    entry('choice', 'items', 'Choose an item from a non-empty list.'),
    entry('shuffle', 'items', 'Return a shuffled shallow copy; the original list stays in order.'),
  ],
  time: [
    entry('seconds', '', 'Monotonic decimal seconds since this interpreter session started or reset.'),
    entry('sleep', 'seconds', 'Wait for 0–86400 seconds; fractional seconds work. Stop and Pause stay responsive.'),
  ],
  files: [
    entry('exists', 'path', 'Whether a file or directory exists. Paths are relative to this source file.'),
    entry('read', 'path', 'Read UTF-8 text (up to 16 MiB), removing an optional UTF-8 BOM.'),
    entry('write', 'path, content', 'Create or overwrite a UTF-8 text file. Parent directories must exist.'),
    entry('append', 'path, content', 'Append UTF-8 text, creating the file if needed. No automatic newline.'),
  ],
};
export const canonicalLibraries = Object.keys(libraries);
for (const item of builtins) item.detailNo = helpNo[item.label];
for (const [module, entries] of Object.entries(libraries)) for (const item of entries) item.detailNo = helpNo[`${module}.${item.label}`] || helpNo[item.label];
const canonicalModule = name => aliases.modules[name] || name;
const canonicalMember = (library, name) => aliases.functions[canonicalModule(library)]?.[name] || name;
const parametersNo = { value:'verdi', prompt:'spørsmål', condition:'vilkår', message:'melding', list:'liste', index:'plass', places:'desimaler', width:'bredde', height:'høyde', title:'tittel', caption:'tekst', label:'tekst', initial:'startverdi', text:'tekst', callback:'funksjon', items:'elementer', minimum:'minste', maximum:'største', base:'grunntall', exponent:'eksponent', separator:'skilletegn', search:'søk', start:'start', count:'antall', replacement:'erstatning', seconds:'sekunder', path:'filnavn', content:'innhold', rate:'antall', key:'tast', color:'farge', radius:'radius', fields:'felt' };
const aliasEntry = (item, label) => ({ ...item, label, locale:'no', canonical:item.label, parameters:item.parameters.map(p => p.replace(/[a-z_]+/g, w => parametersNo[w] || w)), detail:item.detailNo || item.detail, signature:`${label}<${item.parameters.map(p => p.replace(/[a-z_]+/g, w => parametersNo[w] || w)).join(', ')}>` });
for (const [name, entries] of Object.entries(libraries)) {
  const originals = [...entries];
  for (const [alias, original] of Object.entries(aliases.functions[name])) if (alias !== original) entries.push(aliasEntry(originals.find(e => e.label === original), alias));
}
for (const [alias, original] of Object.entries({...aliases.builtins, liste:'list', objekt:'obj'})) builtins.push(aliasEntry(builtins.find(e => e.label === original), alias));
for (const [alias, original] of Object.entries(aliases.modules)) libraries[alias] = libraries[original];
function localizedEntries(entries, locale, map) {
  if (locale === 'all') return entries;
  if (locale === 'en') return entries.filter(e => e.locale !== 'no');
  const norwegian = new Set(Object.keys(map || {})), translated = new Set(Object.values(map || {}));
  return entries.filter(e => norwegian.has(e.label) || !translated.has(e.label)).map(e => e.locale === 'no' ? e : aliasEntry(e, e.label));
}
export function keywordSuggestions(locale='all') {
  if (locale === 'all') return Object.values(keywordGroups).flat();
  const special = { else:'eller', or:'eller', else_or:'eller' };
  return [...new Set(Object.entries(keywordGroups).filter(([name]) => locale !== 'en' || name !== 'else_or').map(([name, names]) => locale === 'en' ? names[0] : special[name] || names.at(-1)))];
}
const id = '[\\p{L}_][\\p{L}\\p{M}\\p{N}_]*';
const re = source => new RegExp(source, 'u');
// Mask strings and comments without changing columns, including incomplete text while typing.
export function mask(line) { return line.replace(/"(?:\\.|[^"\\])*"?|\/\/.*$/g, value => ' '.repeat(value.length)); }
export function analyze(source) {
  const lines = source.split('\n'), scopes = [{ start: 1, end: Infinity, parent: null, symbols: [] }];
  let current = scopes[0];
  for (let i = 0; i < lines.length; i++) {
    const line = mask(lines[i]).trim(), number = i + 1;
    if (/^\.\s*$/.test(line)) { current.end = number; current = current.parent || current; continue; }
    const pub = /^(?:public|offentlig)\b/.test(line), code = line.replace(/^(?:public|offentlig)\s+/, '');
    const fn = code.match(re(`^(?:func|funk)\\s+(${id})\\s*<([^>]*)>`));
    const object = code.match(re(`^(?:obj|objekt)\\s+(${id})\\s*:`));
    let declaration;
    if (fn) {
      declaration = { ...entry(fn[1], fn[2].trim(), 'Function in this file.'), line: number, public: pub };
      current.symbols.push(declaration);
    } else if (object) {
      declaration = { label: object[1], kind: 'class', fields: [], line: number, public: pub, detail: 'Object type' }; current.symbols.push(declaration);
    }
    const variable = code.match(re(`^var\\s+(${id})(?:\\s+(?:as|som)\\s+(${id}))?`));
    if (variable) {
      const constructor = code.match(re(`=\\s*(${id})\\s*<`));
      const fields = ['obj','objekt'].includes(constructor?.[1]) ? [...code.matchAll(new RegExp(`(${id})\\s*=`, 'gu'))].map(m => m[1]).slice(1) : [];
      const control = code.match(re(`=\\s*(${id})\\.(${id})\\s*<`));
      if (control) {
        let scope = current, library;
        while (scope && !library) { library = scope.symbols.find(s => s.label === control[1]); scope = scope.parent; }
        const member = canonicalMember(library?.module, control[2]);
        if (canonicalModule(library?.module) === 'window' && ['label','input','button','checklist','radio_group','listbox','combobox'].includes(member)) {
          control[2] = member;
          fields.push('caption', 'enabled');
          if (control[2] === 'input' || ['radio_group','listbox','combobox'].includes(control[2])) fields.push('value');
          if (['checklist','radio_group','listbox','combobox'].includes(control[2])) fields.push('items');
          if (control[2] === 'checklist') fields.push('selected');
        }
      }
      current.symbols.push({ label: variable[1], kind: 'variable', detail: variable[2] || 'Inferred type', objectType: constructor?.[1], windowControl: fields.includes('caption') && Boolean(control), fields, line: number });
    }
    const use = lines[i].match(re(`^\\s*(?:use|bruk)\\s+("[^"\\r\\n]+"|${id})(?:\\s+(?:as|som)\\s+(${id}))?`));
    if (use && (use[2] || !use[1].startsWith('"'))) current.symbols.push({ label: use[2] || use[1], kind: 'module', module: use[1], line: number, detail: `Library ${use[1]}` });
    if (current.object) {
      const field = code.match(re(`^(${id})\\s+(?:as|som)\\s+(${id})`));
      if (field) current.object.fields.push({ label: field[1], kind: 'field', detail: field[2] });
    }
    if (/:\s*$/.test(code)) {
      if (/^(else|eller)\b/.test(code) && current.parent) { current.end = number - 1; current = current.parent; }
      const child = { start: number, end: Infinity, parent: current, symbols: [], object: object ? declaration : null, library: /^(?:lib|bibliotek)\b/.test(code) };
      scopes.push(child); current = child;
      if (fn) for (const parameter of fn[2].split(',')) {
        const match = parameter.trim().match(re(`^(${id})(?:\\s+(?:as|som)\\s+(${id}))?`));
        if (match) current.symbols.push({ label: match[1], kind: 'variable', detail: match[2] || 'Parameter', line: number });
      }
      const loop = code.match(re(`^for\\s+(${id})`));
      if (loop) current.symbols.push({ label: loop[1], kind: 'variable', detail: 'Loop variable', line: number });
    }
  }
  return scopes;
}
export function visibleSymbols(source, line) {
  const scopes = analyze(source), found = new Map();
  let scope = scopes.filter(s => s.start <= line && s.end >= line).at(-1);
  while (scope) {
    for (const symbol of scope.symbols) if (!found.has(symbol.label) && (symbol.line <= line || ['function','class'].includes(symbol.kind))) found.set(symbol.label, symbol);
    scope = scope.parent;
  }
  return [...found.values()];
}
export function publicExports(source) { return analyze(source).filter(s => s.library).flatMap(s => s.symbols.filter(symbol => symbol.public)); }
function moduleMembers(target, modules) {
  if (Object.hasOwn(libraries, target?.module)) return libraries[target.module];
  return publicExports(modules[target?.label] || '');
}
export function completions(source, line, column, modules = {}, locale = 'all') {
  const original = source.split('\n')[line - 1]?.slice(0, column - 1) || '';
  const colors = colorCompletions(source, line, original, locale);
  if (colors) return colors;
  if (mask(original).trimEnd().length < original.trimEnd().length) return [];
  if (/^\s*(?:use|bruk)\s+[\p{L}_]*$/u.test(original)) return (locale === 'en' ? canonicalLibraries : locale === 'no' ? Object.keys(aliases.modules) : Object.keys(libraries)).map(label => ({ label, kind: 'module', detail: locale === 'no' ? 'Innebygd standardbibliotek (uten nett).' : 'Bundled standard library (offline).' }));
  const symbols = visibleSymbols(source, line), member = original.match(re(`(${id})\\.(${id})?$`));
  if (member) {
    const target = symbols.find(s => s.label === member[1]);
    if (target?.kind === 'module') return Object.hasOwn(libraries, target.module) ? localizedEntries(moduleMembers(target, modules), locale, aliases.functions[canonicalModule(target.module)]) : moduleMembers(target, modules);
    if (target?.fields?.length) return target.fields.map(f => typeof f === 'string' ? { label: target.windowControl && locale === 'no' ? Object.keys(aliases.windowFields).find(a => aliases.windowFields[a] === f) || f : f, kind: 'field', detail: locale === 'no' ? 'Egenskap' : 'Object field' } : f);
    return symbols.find(s => s.kind === 'class' && s.label === target?.objectType)?.fields || [];
  }
  return [...symbols, ...localizedEntries(builtins, locale, {...aliases.builtins, liste:'list', objekt:'obj'}).filter(b => !symbols.some(s => s.label === b.label)), ...keywordSuggestions(locale).map(label => ({ label, kind: 'keyword', detail: locale === 'no' ? 'SLANG-nøkkelord' : 'SLANG keyword' }))];
}
function colorCompletions(source, line, original, locale) {
  // Only offer colors in the first string argument of the bundled drawing API.
  // Masking before the opening quote rules out comments and ordinary text.
  const text = [...original.matchAll(/"(?:\\.|[^"\\])*"?|\/\/.*$/g)].at(-1);
  if (!text || text.index + text[0].length !== original.length || !/^"[\p{L}]*$/u.test(text[0])) return null;
  const quote = text.index;
  const prefix = source.split('\n').slice(0, line);
  prefix[prefix.length - 1] = original.slice(0, quote);
  const call = prefix.map(mask).join('\n').match(re(`(?<![\\p{L}\\p{M}\\p{N}_.])(${id})\\.(${id})\\s*<\\s*$`));
  if (!call) return null;
  const target = visibleSymbols(source, line).find(s => s.label === call[1]);
  if (target?.kind !== 'module' || canonicalModule(target.module) !== 'graphics' || !['fill', 'background'].includes(canonicalMember(target.module, call[2]))) return null;
  const norwegian = Object.keys(aliases.colors), english = [...Object.values(aliases.colors), 'grey'];
  const names = locale === 'no' ? norwegian : locale === 'en' ? english : [...norwegian, ...english];
  return [...new Set(names)].map(label => ({ label, kind: 'color', detail: locale === 'no' ? 'Fargenavn' : 'Color name' }));
}
export function callSignature(source, line, column, modules = {}, locale = 'all') {
  const prefix = source.split('\n').slice(0, line).map((text, i) => mask(i === line - 1 ? text.slice(0, column - 1) : text)).join('\n');
  let depth = 0, commas = 0;
  for (let i = prefix.length - 1; i >= 0; i--) {
    if (prefix[i] === '>') depth++;
    else if (prefix[i] === '<') {
      if (depth) { depth--; continue; }
      const match = prefix.slice(0, i).match(re(`(${id}(?:\\.${id})?)\\s*$`));
      if (!match) return null;
      const name = match[1], member = name.split('.');
      const options = [...visibleSymbols(source, line), ...builtins];
      // Resolve member signatures directly so offsets on multiline calls remain irrelevant.
      let item = options.find(s => s.label === name);
      if (member.length === 2) {
        const library = visibleSymbols(source, line).find(s => s.label === member[0]);
        item = moduleMembers(library, modules).find(s => s.label === member[1]);
      }
      if (locale === 'no' && item?.detailNo) item = aliasEntry(item, item.label);
      return item?.signature ? { ...item, activeParameter: Math.min(commas, Math.max(0, item.parameters.length - 1)) } : null;
    } else if (prefix[i] === ',' && !depth) commas++;
  }
  return null;
}
