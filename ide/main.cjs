const { app, BrowserWindow, ipcMain, dialog, protocol, net, Menu } = require('electron');
const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { spawn } = require('node:child_process');
const { Decoder } = require('./protocol.cjs');
const { normalizeLocale, translate } = require('./i18n.cjs');
let locale = 'en';
const t = message => translate(locale, message);
const base = __dirname;
const development = !app.isPackaged;
const project = development ? path.dirname(base) : base;
const native = process.env.SLANG_BINARY || path.join(project, development ? 'build/native-ninja' : 'bin', process.platform === 'win32' ? 'slang.exe' : 'slang');
if (process.env.SLANG_IDE_PROFILE) app.setPath('userData', path.resolve(process.env.SLANG_IDE_PROFILE));
protocol.registerSchemesAsPrivileged([{ scheme: 'slang-app', privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true } }]);
let window, root = path.join(project, 'examples'), running = null, quitting = false;
const granted = new Set();
const inside = (parent, child) => { const relative = path.relative(parent, child); return !relative.startsWith('..' + path.sep) && relative !== '..' && !path.isAbsolute(relative); };
async function authorized(file) {
  if (typeof file !== 'string') throw new Error('Invalid file path');
  const real = await fs.realpath(file);
  if (!inside(await fs.realpath(root), real) && !granted.has(real)) throw new Error('Choose this file or folder with Open first.');
  return real;
}
const send = event => { if (window && !window.isDestroyed()) window.webContents.send('runner', event); };
const command = text => { if (running?.child.stdin.writable) running.child.stdin.write(text + '\n'); };
function stop() {
  const run = running; if (!run) return;
  command('stop'); run.timer ??= setTimeout(() => run.child.kill(), 1500);
}
function handle(name, fn) {
  ipcMain.handle(name, async (event, ...args) => {
    if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame) throw new Error('Untrusted request');
    return fn(...args);
  });
}
handle('init', async () => ({ root, version: app.getVersion(), platform: process.platform }));
handle('locale', async value => { locale = normalizeLocale(value); return locale; });
handle('folder', async () => {
  const result = await dialog.showOpenDialog(window, { title:t('Open folder'), buttonLabel:t('Open folder'), properties: ['openDirectory'] });
  if (result.canceled) return null;
  root = await fs.realpath(result.filePaths[0]); return root;
});
handle('tree', async directory => {
  const dir = await authorized(directory || root);
  const entries = await fs.readdir(dir, { withFileTypes: true });
  return entries.filter(e => !e.name.startsWith('.') && !['node_modules', 'build', 'dist'].includes(e.name) && (e.isDirectory() || e.isFile())).map(e => ({ name: e.name, path: path.join(dir, e.name), directory: e.isDirectory() })).sort((a,b) => Number(b.directory) - Number(a.directory) || a.name.localeCompare(b.name));
});
async function read(file) {
  const real = await authorized(file), stat = await fs.stat(real);
  if (!stat.isFile() || stat.size > 2 * 1024 * 1024) throw new Error('Open a text file smaller than 2 MB.');
  const text = await fs.readFile(real, 'utf8');
  if (text.includes('\0')) throw new Error('This is a binary file. Choose a SLANG source file.');
  granted.add(real); return { path: real, text, name: path.basename(real) };
}
handle('read', read);
handle('open', async () => {
  const result = await dialog.showOpenDialog(window, { title:t('Open file'), buttonLabel:t('Open file'), properties: ['openFile'], filters: [{ name: t('SLANG source'), extensions: ['slang'] }, { name: t('Text files'), extensions: ['txt', 'md'] }] });
  if (result.canceled) return null;
  const file = await fs.realpath(result.filePaths[0]); granted.add(file); return read(file);
});
handle('save', async data => {
  if (typeof data.text !== 'string' || Buffer.byteLength(data.text) > 2 * 1024 * 1024) throw new Error('Source exceeds 2 MB.');
  let file = data.path;
  if (data.saveAs || !file) {
    const result = await dialog.showSaveDialog(window, { title:t('Save as'), buttonLabel:t('Save'), defaultPath: file || path.join(root, locale === 'no' ? 'uten-navn.slang' : 'untitled.slang'), filters: [{ name: t('SLANG source'), extensions: ['slang'] }] });
    if (result.canceled) return null;
    file = result.filePath;
    // The native save dialog grants precisely the chosen file, including outside the folder.
    try { file = await fs.realpath(file); } catch (e) { if (e.code !== 'ENOENT') throw e; }
    granted.add(file);
  } else file = await authorized(file);
  await fs.writeFile(file, data.text, 'utf8'); granted.add(await fs.realpath(file));
  return { path: file, name: path.basename(file) };
});
handle('confirm', async name => {
  const result = await dialog.showMessageBox(window, { type: 'question', title: t('Unsaved changes'), message: locale === 'no' ? `Lagre endringer i ${String(name).slice(0,200)}?` : `Save changes to ${String(name).slice(0,200)}?`, buttons: ['Save', 'Discard', 'Cancel'].map(t), defaultId: 0, cancelId: 2, noLink: true });
  return ['save','discard','cancel'][result.response];
});
handle('modules', async (file, text) => {
  if (!file) return {};
  const source = await read(file), modules = {};
  if (typeof text === 'string' && text.length <= 2 * 1024 * 1024) source.text = text;
  for (const match of source.text.matchAll(/^\s*(?:use|bruk)\s+"([^"\r\n]+)"\s+(?:as|som)\s+([\p{L}_][\p{L}\p{N}_]*)/gmu)) {
    try { modules[match[2]] = (await read(path.resolve(path.dirname(source.path), match[1]))).text; } catch { /* Missing or outside workspace: no speculative exports. */ }
  }
  return modules;
});
handle('check', async source => {
  if (typeof source !== 'string' || source.length > 2 * 1024 * 1024) return null;
  return new Promise(resolve => {
    const child = spawn(native, ['check', '--json', '--locale', locale, '-'], { windowsHide: true }); let result = '', done = false;
    const finish = value => { if (!done) { done = true; clearTimeout(timer); resolve(value); } };
    const timer = setTimeout(() => { child.kill(); finish(null); }, 3000);
    child.on('error', () => finish(null)); child.stdin.on('error', () => {});
    child.stdout.on('data', data => { result += data; if (result.length > 1048576) child.kill(); });
    child.stderr.resume(); child.on('close', () => { try { finish(JSON.parse(result.trim())); } catch { finish(null); } });
    child.stdin.end(source);
  });
});
handle('run', async data => {
  if (running) throw new Error('Stop the current program first.');
  if (typeof data.text !== 'string' || Buffer.byteLength(data.text) > 2 * 1024 * 1024) throw new Error('Source exceeds 2 MB.');
  const file = data.path ? await authorized(data.path) : path.join(root, 'untitled.slang');
  const temporary = await fs.mkdtemp(path.join(app.getPath('userData'), 'run-'));
  const snapshot = path.join(temporary, 'source.slang'); await fs.writeFile(snapshot, data.text);
  const child = spawn(native, ['ide', file, '--snapshot', snapshot, '--locale', locale, ...(data.debug ? ['--debug'] : [])], { cwd: path.dirname(file), windowsHide: true });
  const decoder = new Decoder(), run = { child, temporary, finished: false, timer: null }; running = run;
  let consoleText = '', consoleTimer = null;
  const flushConsole = () => { clearTimeout(consoleTimer); consoleTimer = null; if (consoleText) { send({ type: 'console', text: consoleText }); consoleText = ''; } };
  child.stdin.on('error', () => {});
  decoder.on('packet', event => {
    if (event.type === 'console') {
      consoleText += event.text;
      if (consoleText.length > 262144) consoleText = '[Earlier output trimmed]\n' + consoleText.slice(-200000);
      consoleTimer ??= setTimeout(flushConsole, 16); return;
    }
    if (['paused','finished','error','state'].includes(event.type)) flushConsole();
    if (event.type === 'ready') {
      for (const entry of data.breakpoints || []) {
        if ((entry.path === null || typeof entry.path === 'string') && Array.isArray(entry.lines)) command(`breakpoints\t${Buffer.from(entry.path || file).toString('base64')}\t${entry.lines.filter(n => Number.isInteger(n) && n > 0).join(',')}`);
      }
      command('start');
    }
    if (event.type === 'finished') run.finished = true;
    send(event);
  });
  child.stdout.on('data', bytes => { try { decoder.push(bytes); } catch (e) { send({ type: 'error', message: e.message }); child.kill(); } });
  child.stderr.on('data', bytes => send({ type: 'console', text: bytes.toString('utf8') }));
  child.on('error', e => send({ type: 'error', message: `Could not start the native runner: ${e.message}` }));
  child.on('close', code => {
    flushConsole();
    clearTimeout(run.timer); if (running === run) running = null;
    if (!run.finished) send({ type: 'finished', status: run.timer ? 'stopped' : 'error', code });
    send({ type: 'exit' }); fs.rm(temporary, { recursive: true, force: true }).catch(() => {});
  });
  return { file };
});
handle('control', async (action, value) => {
  if (action === 'stop') stop();
  else if (['pause','resume','step','frame-ack','canvas-close','eof','keysup'].includes(action)) command(action);
  else if (action === 'input' && typeof value === 'string' && value.length < 65536) command(`input\t${Buffer.from(value).toString('base64')}`);
  else if (action === 'key' && value && /^[a-zA-Z0-9 _-]{1,40}$/.test(value.name)) command(`key\t${value.name}\t${value.down ? 1 : 0}`);
  else if (action === 'window-text' && typeof value === 'string' && Buffer.byteLength(value) <= 16384) command(`window-text\t${Buffer.from(value).toString('base64')}`);
  else if (action === 'window-key' && value && /^[a-zA-Z0-9 _-]{1,40}$/.test(value.name)) command(`window-key\t${value.name}\t${value.shift ? 1 : 0}\t${value.control ? 1 : 0}`);
  else if (action === 'window-click' && value && Number.isInteger(value.x) && Number.isInteger(value.y) && Math.abs(value.x) <= 4096 && Math.abs(value.y) <= 4096) command(`window-click\t${value.x}\t${value.y}`);
  else if (action === 'window-wheel' && Number.isInteger(value) && Math.abs(value) <= 512) command(`window-wheel\t${value}`);
  else if (action === 'breakpoints' && value && typeof value.path === 'string' && Array.isArray(value.lines)) command(`breakpoints\t${Buffer.from(value.path).toString('base64')}\t${value.lines.filter(n => Number.isInteger(n) && n > 0).join(',')}`);
});
handle('close', async () => { quitting = true; window.close(); });
app.whenReady().then(async () => {
  await fs.mkdir(app.getPath('userData'), { recursive: true });
  protocol.handle('slang-app', request => {
    const url = new URL(request.url), file = path.resolve(base, 'dist', '.' + decodeURIComponent(url.pathname));
    if (url.host !== 'app' || !inside(path.join(base, 'dist'), file)) return new Response('Forbidden', { status: 403 });
    return net.fetch(pathToFileURL(file).toString());
  });
  Menu.setApplicationMenu(null);
  window = new BrowserWindow({ width: 1440, height: 920, minWidth: 960, minHeight: 640, title: 'SLANG', backgroundColor: '#f4f6f9', show: false, webPreferences: { preload: path.join(base, 'preload.cjs'), contextIsolation: true, sandbox: true, nodeIntegration: false, spellcheck: false } });
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', event => event.preventDefault());
  window.webContents.session.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
  window.on('close', event => { if (!quitting) { event.preventDefault(); window.webContents.send('closing'); } else stop(); });
  window.once('ready-to-show', () => window.show());
  await window.loadURL('slang-app://app/index.html');
});
app.on('window-all-closed', () => { running?.child.kill(); app.quit(); });
