import { cp, mkdir, copyFile, rename, readFile, writeFile, stat, access, mkdtemp, chmod } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { packager } from '@electron/packager';
import { checkRelease } from './check-release.mjs';
import { packageLayout, archiveDirectory } from './release-utils.mjs';

// Compile on the target OS/architecture: Electron cannot convert a native binary.
const metadata = await checkRelease(), version = metadata.version;
const layout = packageLayout(version, process.platform, process.arch);
const build = path.resolve(process.env.SLANG_BUILD_DIR || 'build/native-ninja');
const native = path.join(build, process.platform === 'win32' ? 'slang.exe' : 'slang');
await access('ide/dist/renderer.js'); await access(native);
if (!execFileSync(native, ['--version'], { encoding: 'utf8' }).startsWith(`slang ${version} `)) throw new Error('Rebuild the native interpreter: its version differs from the release.');
const output = path.resolve(process.env.SLANG_DIST_DIR || 'dist');
const target = path.join(output, layout.name), archive = target + layout.extension;
for (const file of [target,archive]) {
  try { await stat(file); throw new Error(`Package already exists: ${file}. Move it aside or use SLANG_DIST_DIR.`); } catch (e) { if (e.code !== 'ENOENT') throw e; }
}
await mkdir(output, { recursive: true }); await mkdir('build', { recursive: true });
const temporary = await mkdtemp(path.resolve('build/package-'));
const stage = path.join(temporary,'app'); await mkdir(stage);
for (const file of ['package.json','main.cjs','preload.cjs','protocol.cjs','i18n.cjs']) await copyFile(`ide/${file}`, path.join(stage,file));
await cp('ide/dist', path.join(stage,'dist'), { recursive: true });
for (const folder of ['examples','docs']) await cp(folder, path.join(stage,folder), { recursive: true });
for (const file of ['LICENSE','THIRD-PARTY-NOTICES.md']) await copyFile(file,path.join(stage,file));
await mkdir(path.join(stage,'bin')); await copyFile(native,path.join(stage,'bin',path.basename(native)));
if (process.platform !== 'win32') await chmod(path.join(stage,'bin/slang'),0o755);

const packages = await packager({ dir: stage, out: path.join(temporary,'output'), name: 'SLANG',
  platform: process.platform, arch: process.arch, appVersion: version,
  appBundleId: 'org.slang-language.ide', appCategoryType: 'public.app-category.education',
  appCopyright: 'Copyright (c) 2026 SLANG contributors',
  electronVersion: metadata.devDependencies.electron, asar: false, prune: false,
  download: { cacheRoot: path.resolve(process.env.electron_config_cache || 'build/electron-cache') },
});
if (packages.length !== 1) throw new Error('Expected exactly one native platform package.');
await rename(packages[0],target);
const app = path.join(target,layout.resources,'app');
await copyFile('LICENSE',path.join(target,'LICENSE-SLANG.txt'));
await copyFile('examples/graphics/LICENSE',path.join(target,'LICENSE-CC0.txt'));
const notices = (await readFile('THIRD-PARTY-NOTICES.md','utf8')).replace('(LICENSE)','(LICENSE-SLANG.txt)').replace('(examples/graphics/LICENSE)',`(${layout.resources}/app/examples/graphics/LICENSE)`);
await writeFile(path.join(target,'THIRD-PARTY-NOTICES.md'),notices);
const guide = await readFile('packaging/IDE-START-HERE.txt','utf8');
await writeFile(path.join(target,'START-HERE.txt'), `SLANG ${version} — ${process.platform} ${process.arch}\n\nOpen ${layout.entry}. Keep the application bundle/files together.\nNative CLI: ${layout.resources}/app/bin/${path.basename(native)}\nOffline guide: ${layout.resources}/app/docs/ide.md\n\n${guide}`);
if (process.platform === 'win32') {
  await writeFile(path.join(target,'SLANG-console.cmd'), '@echo off\r\n"%~dp0resources\\app\\bin\\slang.exe" repl\r\n');
  await writeFile(path.join(target,'slang.cmd'), '@echo off\r\n"%~dp0resources\\app\\bin\\slang.exe" %*\r\n');
  execFileSync('icacls',[target,'/grant','*S-1-15-2-1:(OI)(CI)(RX)'],{ stdio:'pipe' });
} else {
  await writeFile(path.join(target,'slang'), `#!/bin/sh\nbase=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$base/${layout.resources}/app/bin/slang" "$@"\n`, { mode: 0o755 });
}
const licenses = path.join(target,'licenses'); await mkdir(licenses);
for (const [source,name] of [
  ['node_modules/monaco-editor/LICENSE','Monaco.txt'],
  ['node_modules/monaco-editor/ThirdPartyNotices.txt','Monaco-ThirdPartyNotices.txt'],
  ['node_modules/dompurify/LICENSE','DOMPurify.txt'],
  ['native/assets/OFL.txt','NotoSans-OFL.txt'],
  ['native/vendor/stb/LICENSE','stb_truetype.txt'],
  [path.join(build,'_deps/sdl3-src/LICENSE.txt'),'SDL3.txt'],
  [path.join(build,'_deps/sdl3-src/src/hidapi/LICENSE-bsd.txt'),'HIDAPI.txt'],
  [path.join(build,'_deps/sdl3-src/src/video/yuv2rgb/LICENSE'),'yuv2rgb.txt'],
  [path.join(build,'_deps/sdl3-src/src/video/stb_image.h'),'stb_image.h'],
  [path.join(build,'_deps/sdl3-src/src/video/miniz.h'),'miniz.h'],
]) await copyFile(source,path.join(licenses,name));
// Notices also live inside the .app, which users may move without its outer folder.
await cp(licenses,path.join(app,'licenses'),{ recursive:true });
if (process.platform === 'darwin') {
  // Ad-hoc signing supports Apple Silicon execution; this is not publisher signing.
  execFileSync('codesign',['--force','--deep','--sign','-','--preserve-metadata=entitlements,flags',path.join(target,'SLANG.app')],{ stdio:'inherit' });
  execFileSync('codesign',['--verify','--deep','--strict',path.join(target,'SLANG.app')],{ stdio:'inherit' });
}
await archiveDirectory(target,archive,process.platform);
await writeFile('build/ide-package.json',JSON.stringify({ version,platform:process.platform,arch:process.arch,directory:target,archive,executable:path.join(target,layout.executable) },null,2)+'\n');
console.log(`Portable IDE: ${target}\nArchive: ${archive}\n${Math.round((await stat(archive)).size/1048576)} MiB; no separate runtime required.`);
