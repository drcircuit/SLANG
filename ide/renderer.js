import * as monaco from '../node_modules/monaco-editor/esm/vs/editor/editor.api.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/suggest/browser/suggestController.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/snippet/browser/snippetController2.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/parameterHints/browser/parameterHints.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/hover/browser/hoverContribution.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/find/browser/findController.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/comment/browser/comment.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/clipboard/browser/clipboard.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/bracketMatching/browser/bracketMatching.js';
import '../node_modules/monaco-editor/esm/vs/editor/contrib/gotoError/browser/gotoError.js';
import { keywordGroups, builtins, completions, callSignature } from './language.mjs';
import { palettes, installThemes } from './themes.mjs';
import { normalizeLocale, translate } from './i18n.cjs';
const $ = id => document.getElementById(id), api = window.slang;
let codeLocale = 'en';
try { codeLocale = normalizeLocale(localStorage.getItem('slang.language')); } catch {}
const t = message => translate(codeLocale, message);
globalThis.MonacoEnvironment = { getWorker: () => new Worker(new URL('./editor.worker.js', location.href), { type: 'module' }) };
monaco.languages.register({ id: 'slang', extensions: ['.slang'] });
const words = Object.values(keywordGroups).flat();
monaco.languages.setMonarchTokensProvider('slang', {
  unicode: true, defaultToken: 'identifier', keywords: words, builtins: builtins.map(b => b.label),
  tokenizer: { root: [
    [/^#(?:no|en)\b.*$/, 'keyword'],
    [/\/\/.*$/, 'comment'], [/"/, 'string', '@string'],
    [/[\p{L}_][\p{L}\p{M}\p{N}_]*/u, { cases: { '@keywords': 'keyword', '@builtins': 'predefined', '@default': 'identifier' } }],
    [/\d+\.\d+|\d+/, 'number'], [/^\s*\.\s*$/, 'keyword'], [/[<>:=+*/%(),.-]/, 'delimiter'],
  ], string: [[/[^\\"]+/, 'string'], [/\\./, 'string.escape'], [/"/, 'string', '@pop']] },
});
monaco.languages.setLanguageConfiguration('slang', {
  comments: { lineComment: '//' }, brackets: [['(',')']],
  autoClosingPairs: [{ open: '"', close: '"', notIn: ['string','comment'] }, { open: '(', close: ')' }],
  surroundingPairs: [{ open: '"', close: '"' }, { open: '(', close: ')' }],
  wordPattern: /[\p{L}_][\p{L}\p{M}\p{N}_]*/u,
  indentationRules: { increaseIndentPattern: /:\s*(?:\/\/.*)?$/, decreaseIndentPattern: /^\s*(?:\.|else\b|eller\b)/ },
});
const appearance = installThemes(monaco);
for (const [value, palette] of Object.entries(palettes)) $('theme-palette').add(new Option(palette.label, value));
$('theme-mode').value = appearance.current().mode; $('theme-palette').value = appearance.current().palette;
for (const id of ['theme-mode','theme-palette']) $(id).addEventListener('change', () => appearance.apply({ mode: $('theme-mode').value, palette: $('theme-palette').value }));
let documents = [], active, root, platform = 'win32', state = 'idle', processActive = false, pausedAt = null, untitled = 0, toastTimer, checkTimer, checking = false;
const key = path => platform === 'win32' ? path?.replaceAll('\\','/').toLowerCase() : path;
const editor = monaco.editor.create($('editor'), { language: 'slang', theme: 'slang-appearance', automaticLayout: true, minimap: { enabled: false }, fontFamily: 'Cascadia Code, Consolas, monospace', fontSize: 14, lineHeight: 23, padding: { top: 15, bottom: 20 }, scrollBeyondLastLine: false, glyphMargin: true, folding: false, tabSize: 4, insertSpaces: true, autoIndentOnPaste: false, quickSuggestions: { other: true, comments: false, strings: false }, wordBasedSuggestions: 'off', suggest: { showWords: false }, inlineSuggest: { enabled: false }, renderLineHighlight: 'all', overviewRulerLanes: 0, hideCursorInOverviewRuler: true, contextmenu: false, unicodeHighlight: { ambiguousCharacters: false, nonBasicASCII: false }, ariaLabel: 'SLANG code editor' });
function toast(message) { $('toast').textContent = message; $('toast').hidden = false; clearTimeout(toastTimer); toastTimer = setTimeout(() => { $('toast').hidden = true; }, 6500); }
async function safe(fn) { try { return await fn(); } catch (e) { toast(t(e.message.replace(/^Error invoking remote method '[^']+': Error: /, ''))); } }
function button(id, fn) { $(id).addEventListener('click', () => safe(fn)); }
const kindMap = { function: monaco.languages.CompletionItemKind.Function, variable: monaco.languages.CompletionItemKind.Variable, field: monaco.languages.CompletionItemKind.Field, class: monaco.languages.CompletionItemKind.Class, module: monaco.languages.CompletionItemKind.Module, keyword: monaco.languages.CompletionItemKind.Keyword, color: monaco.languages.CompletionItemKind.Color };
monaco.languages.registerCompletionItemProvider('slang', { triggerCharacters: ['.', '"'], provideCompletionItems(model, position) {
  const doc = documents.find(d => d.model === model), word = model.getWordUntilPosition(position);
  const range = new monaco.Range(position.lineNumber, word.startColumn, position.lineNumber, word.endColumn);
  const colorRange = new monaco.Range(position.lineNumber, word.startColumn, position.lineNumber, model.getWordAtPosition(position)?.endColumn ?? word.endColumn);
  const seen = new Set();
  return { suggestions: completions(model.getValue(), position.lineNumber, position.column, doc?.modules, codeLocale).filter(item => { if (seen.has(item.label)) return false; seen.add(item.label); return true; }).map(item => ({ label: item.label, kind: kindMap[item.kind], detail: item.signature || t(item.detail), documentation: t(item.detail), range: item.kind === 'color' ? colorRange : range, insertText: item.label, sortText: `${item.kind === 'keyword' ? '2' : '1'}${item.label}` })) };
} });
monaco.languages.registerSignatureHelpProvider('slang', { signatureHelpTriggerCharacters: ['<', ','], signatureHelpRetriggerCharacters: ['>'], provideSignatureHelp(model, position) {
  const item = callSignature(model.getValue(), position.lineNumber, position.column, documents.find(d => d.model === model)?.modules, codeLocale);
  return item ? { value: { signatures: [{ label: item.signature, documentation: item.detail, parameters: item.parameters.map(label => ({ label })) }], activeSignature: 0, activeParameter: item.activeParameter }, dispose() {} } : null;
} });
monaco.languages.registerHoverProvider('slang', { provideHover(model, position) {
  const word = model.getWordAtPosition(position); if (!word) return null;
  const item = completions(model.getValue(), position.lineNumber, word.endColumn, documents.find(d => d.model === model)?.modules, codeLocale).find(s => s.label === word.word);
  return item ? { contents: [{ value: `**${item.signature || item.label}**\n\n${item.detail || ''}` }] } : null;
} });
function renderTabs() {
  $('tabs').replaceChildren();
  for (const doc of documents) {
    const tab = document.createElement('button'); tab.className = `tab${doc === active ? ' selected' : ''}`; tab.setAttribute('role', 'tab'); tab.setAttribute('aria-selected', String(doc === active)); tab.title = doc.path || doc.name;
    const label = document.createElement('span'); label.textContent = doc.name + (doc.dirty ? ' •' : ''); tab.append(label);
    const close = document.createElement('span'); close.className = 'close-tab'; close.textContent = '×'; close.setAttribute('aria-label', `${t('Close')} ${doc.name}`); close.addEventListener('click', event => { event.stopPropagation(); safe(() => closeDocument(doc)); }); tab.append(close);
    tab.addEventListener('click', () => activate(doc)); $('tabs').append(tab);
  }
  if (active) { $('breadcrumb').textContent = active.path || `${root} / ${active.name} (${t('unsaved')})`; $('file-status').textContent = active.dirty ? t('Unsaved changes') : 'SLANG'; document.title = `${active.dirty ? '• ' : ''}${active.name} — SLANG`; }
  document.querySelectorAll('.tree-row').forEach(row => row.classList.toggle('active', key(row.dataset.path) === key(active?.path)));
}
function activate(doc) {
  if (active) active.view = editor.saveViewState(); active = doc; editor.setModel(doc.model); if (doc.view) editor.restoreViewState(doc.view);
  renderTabs(); decorations(); scheduleCheck(); editor.focus();
}
function addDocument(data) {
  const existing = data.path && documents.find(d => key(d.path) === key(data.path)); if (existing) { activate(existing); return existing; }
  const model = monaco.editor.createModel(data.text, 'slang', monaco.Uri.parse(`inmemory://slang/${++untitled}/${encodeURIComponent(data.name)}`));
  const doc = { ...data, model, saved: model.getValue(), dirty: !data.path && data.text !== '', breakpoints: [], markerIds: [], modules: {} }; documents.push(doc);
  model.onDidChangeContent(() => { doc.dirty = model.getValue() !== doc.saved || (!doc.path && model.getValue() !== ''); doc.breakpoints = doc.markerIds.map(id => model.getDecorationRange(id)?.startLineNumber).filter(Boolean); renderTabs(); scheduleCheck(); });
  if (doc.path) api.modules(doc.path).then(modules => { doc.modules = modules; }).catch(() => {});
  activate(doc); return doc;
}
async function save(doc = active, saveAs = false) {
  if (!doc) return false;
  const result = await api.save({ path: doc.path, text: doc.model.getValue(), saveAs }); if (!result) return false;
  Object.assign(doc, result); doc.saved = doc.model.getValue(); doc.dirty = false; renderTabs(); await refreshTree(); doc.modules = await api.modules(doc.path); return true;
}
async function discardOrSave(doc) { if (!doc.dirty) return true; const action = await api.confirm(doc.name); return action === 'save' ? save(doc) : action === 'discard'; }
async function closeDocument(doc) {
  if (!await discardOrSave(doc)) return;
  documents = documents.filter(d => d !== doc); if (active === doc) { active = null; if (documents.length) activate(documents.at(-1)); else addDocument({ name: 'untitled.slang', text: '', path: null }); }
  doc.model.dispose(); renderTabs();
}
async function openFile(path) { const existing = documents.find(d => key(d.path) === key(path)); if (existing) activate(existing); else addDocument(await api.read(path)); }
const expanded = new Set();
async function refreshTree() {
  $('folder-name').textContent = root.split(/[\\/]/).at(-1); $('folder-name').title = root; const fragment = document.createDocumentFragment();
  async function children(directory, depth, target) {
    const entries = await api.tree(directory);
    for (const entry of entries) {
      const row = document.createElement('button'); row.className = 'tree-row'; row.style.paddingLeft = `${14 + depth * 14}px`; row.dataset.path = entry.path; row.title = entry.path;
      const icon = document.createElement('span'); icon.className = 'file-icon'; icon.textContent = entry.directory ? expanded.has(entry.path) ? '▾' : '▸' : entry.name.endsWith('.slang') ? 's.' : '·';
      const name = document.createElement('span'); name.className = 'name'; name.textContent = entry.name; row.append(icon,name); target.append(row);
      row.addEventListener('click', () => safe(async () => { if (entry.directory) { if (expanded.has(entry.path)) expanded.delete(entry.path); else expanded.add(entry.path); await refreshTree(); } else await openFile(entry.path); }));
      if (entry.directory && expanded.has(entry.path)) await children(entry.path, depth + 1, target);
    }
  }
  await children(root, 0, fragment); $('tree').replaceChildren(fragment); renderTabs();
}
function scheduleCheck() { clearTimeout(checkTimer); checkTimer = setTimeout(check, 400); }
async function check() {
  const doc = active; if (!doc) return;
  if (checking) { scheduleCheck(); return; }
  checking = true; const version = doc.model.getVersionId();
  try {
    const result = await api.check(doc.model.getValue());
    if (doc.path && !doc.model.isDisposed()) doc.modules = await api.modules(doc.path, doc.model.getValue()).catch(() => ({}));
    if (doc.model.isDisposed() || version !== doc.model.getVersionId()) return;
    const markers = (result?.diagnostics || []).map(d => ({ severity: monaco.MarkerSeverity.Error, message: d.message, code: d.code, startLineNumber: d.start?.line || 1, startColumn: utf16Column(doc.model, d.start?.line || 1, d.start?.column || 1), endLineNumber: d.end?.line || d.start?.line || 1, endColumn: utf16Column(doc.model, d.end?.line || 1, d.end?.column || 2) }));
    monaco.editor.setModelMarkers(doc.model, 'syntax', markers);
    if (active === doc) $('syntax-status').textContent = !result ? t('Syntax checker unavailable') : result.success ? t('✓ Syntax OK') : codeLocale === 'no' ? `${markers.length} syntaksfeil - pek på understrekingen` : `${markers.length} syntax error — hover the underline`;
  } finally { checking = false; }
}
function utf16Column(model, line, column) { if (line > model.getLineCount()) return 1; return [...model.getLineContent(line)].slice(0, column - 1).join('').length + 1; }
let debugMarkers = [];
function decorations() {
  if (!active) return;
  for (const doc of documents) doc.markerIds = doc.model.deltaDecorations(doc.markerIds, doc.breakpoints.map(line => ({ range: new monaco.Range(line,1,line,1), options: { glyphMarginClassName: 'breakpoint-glyph', glyphMarginHoverMessage: { value: t('Breakpoint — click to remove') }, stickiness: monaco.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges } })));
  debugMarkers = editor.deltaDecorations(debugMarkers, pausedAt && (key(active.path) === key(pausedAt.file) || (!active.path && pausedAt.document === active)) ? [{ range: new monaco.Range(pausedAt.line,1,pausedAt.line,1), options: { isWholeLine: true, className: 'debug-line', glyphMarginClassName: 'debug-line-glyph' } }] : []);
}
editor.onMouseDown(event => {
  if (event.target.type !== monaco.editor.MouseTargetType.GUTTER_GLYPH_MARGIN || !active) return;
  const line = event.target.position.lineNumber;
  active.breakpoints = active.breakpoints.includes(line) ? active.breakpoints.filter(n => n !== line) : [...active.breakpoints, line]; decorations();
  if (processActive) api.control('breakpoints', { path: active.path || runDocument?.runFile, lines: active.breakpoints });
});
function updateCursorStatus(position = editor.getPosition()) { if (!position) return; $('cursor-status').textContent = codeLocale === 'no' ? `Linje ${position.lineNumber}, kolonne ${position.column}` : `Ln ${position.lineNumber}, Col ${position.column}`; }
editor.onDidChangeCursorPosition(e => updateCursorStatus(e.position));
function output(text, className = '', location = null) {
  const console = $('console'), follow = console.scrollHeight - console.scrollTop - console.clientHeight < 40;
  const node = document.createElement(location ? 'button' : 'span'); node.className = className; node.textContent = text;
  if (location) node.addEventListener('click', () => safe(() => showLocation(location))); console.append(node);
  while (console.childNodes.length > 1500 || console.textContent.length > 200000) console.firstChild.remove();
  if (follow) console.scrollTop = console.scrollHeight;
}
function updateControls() {
  $('run').disabled = processActive && state !== 'paused'; $('run').innerHTML = '<span>▶</span> ' + t(state === 'paused' ? 'Continue' : 'Run');
  $('stop').disabled = !processActive; $('pause').disabled = !processActive || state === 'starting' || state === 'stopping';
  $('pause').innerHTML = '<span>' + (state === 'paused' ? '▶' : 'Ⅱ') + '</span> ' + t(state === 'paused' ? 'Resume' : 'Pause');
  $('debug').disabled = processActive; $('step').disabled = state !== 'paused'; $('close-canvas').disabled = !processActive || !$('canvas').classList.contains('visible');
  const input = state === 'input'; for (const id of ['console-input','send-input','eof']) $(id).disabled = !input;
  $('run-state').className = `state ${state}`; $('run-state').lastElementChild.textContent = t({ idle: 'Ready', running: 'Running', starting: 'Starting', stopping: 'Stopping', paused: 'Paused', input: 'Waiting for input' }[state] || state);
  editor.updateOptions({ readOnly: processActive });
}
let runDocument = null;
async function start(debug = false) {
  if (state === 'paused') { await api.control('resume'); return; }
  if (processActive || !active) return;
  processActive = true; state = 'starting'; runDocument = active; pausedAt = null; decorations(); updateControls();
  $('console').replaceChildren(); $('console-status').textContent = debug ? t('Debug session') : t('Program output');
  $('variables').replaceChildren(); $('canvas').classList.remove('visible'); $('canvas-empty').hidden = false; $('canvas-size').textContent = t('SDL graphics'); $('debug-location').textContent = t('Debugger');
  for (const doc of documents) monaco.editor.setModelMarkers(doc.model, 'runtime', []);
  try {
    const result = await api.run({ path: active.path, text: active.model.getValue(), debug, breakpoints: documents.filter(d => d.path || d === active).map(d => ({ path: d.path, lines: d.breakpoints })) }); runDocument.runFile = result.file;
  } catch (e) { processActive = false; state = 'idle'; updateControls(); throw e; }
}
async function showLocation(location) {
  let doc = documents.find(d => key(d.path) === key(location.file));
  if (!doc && runDocument && (key(runDocument.runFile) === key(location.file) || key(runDocument.path) === key(location.file))) doc = runDocument;
  if (doc) activate(doc); else { await openFile(location.file); doc = active; }
  editor.setPosition({ lineNumber: location.line || 1, column: utf16Column(doc.model, location.line || 1, location.column || 1) }); editor.revealLineInCenterIfOutsideViewport(location.line || 1); decorations();
}
api.onRunner(event => {
  if (event.type === 'console') output(event.text);
  else if (event.type === 'frame') {
    try {
      const canvas = $('canvas'); if (canvas.width !== event.width || canvas.height !== event.height) { canvas.width = event.width; canvas.height = event.height; }
      const pixels = new Uint8ClampedArray(event.pixels.buffer, event.pixels.byteOffset, event.pixels.byteLength);
      canvas.getContext('2d').putImageData(new ImageData(pixels, event.width, event.height), 0, 0);
      canvas.classList.add('visible'); $('canvas-empty').hidden = true; $('canvas-size').textContent = `${event.width} × ${event.height}`; $('close-canvas').disabled = !processActive;
    } finally { api.control('frame-ack'); }
  } else if (event.type === 'state') {
    state = event.state; if (state !== 'paused') { pausedAt = null; decorations(); }
    updateControls(); if (state === 'input') $('console-input').focus();
  } else if (event.type === 'paused') {
    state = 'paused'; pausedAt = { ...event, document: runDocument }; updateControls();
    $('debug-location').textContent = `${t(event.reason)} · ${codeLocale === 'no' ? 'linje' : 'line'} ${event.line}`; $('variables').replaceChildren();
    for (const variable of event.variables) {
      const row = document.createElement('div'); row.className = 'variable';
      const name = document.createElement('span'); name.className = 'variable-name'; name.textContent = variable.name;
      const type = document.createElement('small'); type.className = 'variable-type'; type.textContent = variable.type; name.append(type);
      const value = document.createElement('span'); value.className = 'variable-value'; value.textContent = variable.value; row.append(name,value); $('variables').append(row);
    }
    if (!event.variables.length) { const note = document.createElement('p'); note.className = 'empty-note'; note.textContent = t('No variables at this point. Step to the next statement to inspect its scope.'); $('variables').append(note); }
    safe(() => showLocation(event));
  } else if (event.type === 'error') {
    output(`${event.file ? `${event.file.split(/[\\/]/).at(-1)}:${event.line}: ` : ''}${event.message}\n`, event.file ? 'console-link' : 'console-error', event.file ? event : null);
    const doc = documents.find(d => key(d.path) === key(event.file)) || ((!event.file || key(runDocument?.runFile) === key(event.file)) ? runDocument : null);
    if (doc && event.line) monaco.editor.setModelMarkers(doc.model, 'runtime', [{ severity: monaco.MarkerSeverity.Error, message: event.message, startLineNumber: event.line, endLineNumber: event.line, startColumn: utf16Column(doc.model, event.line, event.column || 1), endColumn: utf16Column(doc.model, event.line, (event.column || 1) + 1) }]);
  } else if (event.type === 'finished') {
    output(`\n${t({ success: 'Program finished.', stopped: 'Program stopped.', error: 'Program ended with an error.' }[event.status])}\n`, 'console-info'); $('console-status').textContent = t(event.status);
  } else if (event.type === 'exit') { processActive = false; state = 'idle'; pausedAt = null; decorations(); updateControls(); }
});
button('run', () => start()); button('debug', () => start(true)); button('step', () => api.control('step'));
button('stop', async () => { state = 'stopping'; updateControls(); await api.control('stop'); });
button('pause', () => api.control(state === 'paused' ? 'resume' : 'pause'));
button('new', () => addDocument({ path: null, name: `untitled${untitled ? '-' + untitled : ''}.slang`, text: '' }));
button('open', async () => { const file = await api.open(); if (file) addDocument(file); });
button('folder', async () => { const folder = await api.folder(); if (folder) { root = folder; expanded.clear(); await refreshTree(); } });
button('refresh', refreshTree); button('save', () => save()); button('save-as', () => save(active, true)); button('clear-console', () => $('console').replaceChildren());
button('close-canvas', () => api.control('canvas-close')); button('eof', () => api.control('eof'));
$('input-form').addEventListener('submit', event => { event.preventDefault(); if (state !== 'input') return; const value = $('console-input').value; output(value + '\n', 'console-input-line'); $('console-input').value = ''; safe(() => api.control('input', value)); });
const keyNames = { ArrowRight: 'Right', ArrowLeft: 'Left', ArrowUp: 'Up', ArrowDown: 'Down', ' ': 'Space', Escape: 'Escape', Enter: 'Return', Backspace: 'Backspace', Delete: 'Delete', Home: 'Home', End: 'End', PageUp: 'PageUp', PageDown: 'PageDown', Tab: 'Tab', Shift: 'Left Shift', Control: 'Left Ctrl', Alt: 'Left Alt' };
let windowMode = false;
// A real text-input element supplies composition/dead keys/paste; the SDL
// control owns the displayed value and caret. No keyboard-layout guessing.
const windowInput = document.createElement('textarea');
windowInput.id = 'window-input'; windowInput.setAttribute('aria-label', t('Program window keyboard input'));
windowInput.autocomplete = 'off'; windowInput.spellcheck = false; windowInput.tabIndex = -1;
Object.assign(windowInput.style, { position: 'absolute', width: '1px', height: '1px', opacity: '0', padding: '0', border: '0' });
$('canvas-stage').append(windowInput);
api.onRunner(event => {
  if (event.type === 'window-mode') windowMode = event.enabled;
  if (event.type === 'exit') windowMode = false;
  if (!windowMode) windowInput.value = '';
});
const commitWindowText = () => { if (windowMode && processActive && windowInput.value) { api.control('window-text', windowInput.value); windowInput.value = ''; } };
windowInput.addEventListener('input', event => { if (!event.isComposing) commitWindowText(); });
windowInput.addEventListener('compositionend', () => queueMicrotask(commitWindowText));
windowInput.addEventListener('keydown', event => {
  if (!windowMode || !processActive || event.isComposing || /^F\d+$/.test(event.key)) return;
  const mod = event.ctrlKey || event.metaKey;
  if (mod && event.key.toLowerCase() === 'v') { event.stopPropagation(); return; }
  if (mod || keyNames[event.key] || ['Delete','Home','End','PageUp','PageDown'].includes(event.key)) {
    event.preventDefault(); event.stopPropagation();
    api.control('window-key', { name: keyNames[event.key] || event.key.toUpperCase(), shift: event.shiftKey, control: mod });
  }
});
$('canvas').addEventListener('pointerdown', event => {
  if (!windowMode || !processActive || event.button !== 0) return;
  event.preventDefault(); const rect = $('canvas').getBoundingClientRect();
  api.control('window-click', { x: Math.floor((event.clientX - rect.left) * $('canvas').width / rect.width), y: Math.floor((event.clientY - rect.top) * $('canvas').height / rect.height) });
  windowInput.focus({ preventScroll: true });
});
$('canvas').addEventListener('wheel', event => {
  if (!windowMode || !processActive) return;
  event.preventDefault(); api.control('window-wheel', Math.max(-512, Math.min(512, Math.round(event.deltaY))));
}, { passive: false });
for (const eventName of ['keydown','keyup']) $('canvas').addEventListener(eventName, event => { if (!processActive || windowMode || event.ctrlKey || event.metaKey || /^F\d+$/.test(event.key)) return; event.preventDefault(); if (event.repeat) return; api.control('key', { name: keyNames[event.key] || event.key, down: eventName === 'keydown' }); });
$('canvas').addEventListener('blur', () => api.control('keysup'));
$('canvas').addEventListener('focus', () => { $('canvas-hint').textContent = t('Keyboard connected · click the editor to return'); });
$('canvas').addEventListener('blur', () => { $('canvas-hint').textContent = t('Click the canvas to send keyboard input'); });
window.addEventListener('blur', () => api.control('keysup'));
window.addEventListener('keydown', event => {
  const mod = event.ctrlKey || event.metaKey;
  let action;
  if (event.key === 'F5') action = event.shiftKey ? () => $('stop').click() : () => start(mod);
  else if (event.key === 'F6') action = () => !$('pause').disabled && $('pause').click();
  else if (event.key === 'F11') action = () => !$('step').disabled && $('step').click();
  else if (mod && event.key.toLowerCase() === 's') action = () => save(active, event.shiftKey);
  else if (mod && event.key.toLowerCase() === 'o') action = () => $('open').click();
  else if (mod && event.key.toLowerCase() === 'n') action = () => $('new').click();
  else if (mod && event.key.toLowerCase() === 'w') action = () => closeDocument(active);
  if (action) { event.preventDefault(); event.stopPropagation(); safe(action); }
}, true);
let closing = false;
api.onClosing(async () => { if (closing) return; closing = true; try { for (const doc of documents) if (!await discardOrSave(doc)) return; await api.close(); } finally { closing = false; } });
function splitter(id, variable, dimension, direction, min, max) {
  const handle = $(id); let initial, origin;
  const size = () => parseFloat(getComputedStyle(document.documentElement).getPropertyValue(variable));
  const apply = value => document.documentElement.style.setProperty(variable, `${Math.max(min, Math.min(max(), value))}px`);
  handle.addEventListener('pointerdown', event => { initial = size(); origin = event[dimension]; handle.setPointerCapture(event.pointerId); });
  handle.addEventListener('pointermove', event => { if (handle.hasPointerCapture(event.pointerId)) apply(initial + (event[dimension] - origin) * direction); });
  handle.addEventListener('pointerup', event => { if (handle.hasPointerCapture(event.pointerId)) handle.releasePointerCapture(event.pointerId); });
  handle.addEventListener('keydown', event => { if (['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(event.key)) { event.preventDefault(); apply(size() + (['ArrowRight','ArrowDown'].includes(event.key) ? 15 : -15) * direction); } });
}
splitter('file-split','--files','clientX',1,150,() => Math.min(350, innerWidth - 800));
splitter('canvas-split','--output','clientX',-1,240,() => Math.min(650, innerWidth - 650));
splitter('console-split','--console','clientY',-1,115,() => innerHeight - 370);
const drawing = `// A little color, a little code.\nuse graphics as g\n\ng.canvas<640, 480, "Hello, canvas!">\ng.background<"#18243A">\ng.fill<"#8ED6C8">\ng.circle<320, 220, 100>\ng.fill<"#FFD166">\ng.circle<365, 185, 38>\ng.fill<"#FFFFFF">\ng.line<160, 350, 480, 350>\n\nprint<"Look what you made!">\n`;
const drawingNo = '#no\n// Litt farge, litt kode.\nbruk grafikk som g\ng.lerret<640, 480, "Hei, lerret!">\ng.bakgrunn<"#18243A">\ng.fyll<"#8ED6C8">\ng.sirkel<320, 220, 100>\ng.fyll<"#FFD166">\ng.sirkel<365, 185, 38>\ng.fyll<"#FFFFFF">\ng.linje<160, 350, 480, 350>\nskriv<"Se hva du har laget!">\n';
button('drawing-example', () => addDocument({ path: null, name: codeLocale === 'no' ? 'min-tegning.slang' : 'my-drawing.slang', text: codeLocale === 'no' ? drawingNo : drawing }));
// Capture only app-owned chrome. Editor/source, filenames and program output
// are deliberately excluded, including when the language changes mid-session.
const chromeText = [], chromeAttributes = [];
const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
while (walker.nextNode()) {
  const node = walker.currentNode;
  if (!node.parentElement.closest('#editor,#console,#tabs,#tree,#variables,script,.quick-help') && node.textContent.trim()) chromeText.push([node, node.textContent]);
}
for (const element of document.querySelectorAll('[title],[aria-label],[placeholder]')) {
  if (element.closest('#editor,#tabs,#tree')) continue;
  for (const attr of ['title','aria-label','placeholder']) if (element.hasAttribute(attr)) chromeAttributes.push([element, attr, element.getAttribute(attr)]);
}
const helpEnglish = document.querySelector('.quick-help').innerHTML;
function applyLanguage() {
  document.documentElement.lang = codeLocale === 'no' ? 'nb' : 'en';
  for (const [node, original] of chromeText) if (node.isConnected) node.textContent = original.replace(original.trim(), t(original.trim()));
  for (const [element, attr, original] of chromeAttributes) element.setAttribute(attr, t(original));
  document.querySelector('.quick-help').innerHTML = codeLocale === 'no' ? '<summary>Litt hjelp</summary><p><code>skriv&lt;"Hei"&gt;</code> skriver til konsollen.</p><p><code>bruk grafikk som g</code> henter tegneverktøy.</p><p>Åpne en blokk med <code>:</code> og lukk med <code>.</code></p><p><kbd>Ctrl</kbd> + <kbd>Space</kbd> gir kodeforslag.<br><kbd>F5</kbd> kjører · <kbd>F11</kbd> går ett steg.</p>' : helpEnglish;
  $('language-locale').value = codeLocale;
  editor.updateOptions({ ariaLabel:t('SLANG code editor') });
  renderTabs(); updateControls(); updateCursorStatus(); scheduleCheck();
}
$('language-locale').addEventListener('change', () => safe(async () => {
  codeLocale = normalizeLocale($('language-locale').value);
  try { localStorage.setItem('slang.language', codeLocale); } catch {}
  await api.locale(codeLocale); applyLanguage(); editor.trigger('language', 'hideSuggestWidget');
}));
await safe(async () => {
  await api.locale(codeLocale);
  const info = await api.init(); root = info.root; platform = info.platform; $('version').textContent = `v${info.version}`;
  await refreshTree();
  addDocument({ path: null, name: 'welcome.slang', text: '// Welcome to SLANG. Make something yours.\n// Press Run, or F5, to try this program.\n\nvar name = "verden"\nprint<"Hei, " + name + "!">\n\n// A block starts with : and ends with .\nrepeat 3 times:\n    print<"I can make things with code.">\n.\n' });
  if (codeLocale === 'no') { active.name = 'velkommen.slang'; active.model.setValue('#no\n// Velkommen til SLANG.\n// Trykk Kjør eller F5.\nvar navn = "verden"\nskriv<"Hei, " + navn + "!">\ngjenta 3 ganger:\n    skriv<"Jeg kan lage ting med kode.">\n.\n'); }
  // The starter is a template, not an unsaved user edit.
  active.saved = active.model.getValue(); active.dirty = false; applyLanguage();
});
