import { execFileSync, spawnSync } from 'node:child_process';
import { readdir } from 'node:fs/promises';
import path from 'node:path';
import { checkRelease } from './check-release.mjs';
const {version} = await checkRelease(), tag = process.env.RELEASE_TAG;
if (tag !== `v${version}`) throw new Error('Release tag/version mismatch');
const directory = path.resolve(process.argv[2] || 'release-assets');
const names = (await readdir(directory)).filter(name => /\.(zip|tar\.gz)$/.test(name));
if (names.length !== 8 || !names.every(name => name.includes(`-${version}-`))) throw new Error('Expected eight matching archives: IDE and CLI for all four targets.');
const assets = [...names,'SHA256SUMS.txt'].map(name => path.join(directory,name));
const existing = spawnSync('gh',['release','view',tag,'--json','isDraft'],{encoding:'utf8'});
if (existing.status === 0) {
  if (!JSON.parse(existing.stdout).isDraft) throw new Error('This release is already published; refusing to replace its assets.');
  execFileSync('gh',['release','upload',tag,...assets,'--clobber'],{stdio:'inherit'});
} else {
  execFileSync('gh',['release','create',tag,...assets,'--verify-tag','--draft','--generate-notes','--title',`SLANG ${version}`,'--notes-file','.github/release-notes.md',...(version.includes('-') ? ['--prerelease'] : [])],{stdio:'inherit'});
}
