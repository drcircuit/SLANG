import { build } from 'esbuild';
import { mkdir, copyFile } from 'node:fs/promises';
await mkdir('ide/dist', { recursive: true });
await build({ entryPoints: { renderer: 'ide/renderer.js', 'editor.worker': 'node_modules/monaco-editor/esm/vs/editor/editor.worker.js' }, outdir: 'ide/dist', bundle: true, format: 'esm', target: 'chrome144', sourcemap: false, minify: true, loader: { '.ttf': 'file' }, logLevel: 'info' });
await Promise.all(['index.html', 'style.css'].map(file => copyFile(`ide/${file}`, `ide/dist/${file}`)));
