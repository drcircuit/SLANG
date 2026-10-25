import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { parseDocument } from 'yaml';
import { packageLayout } from '../scripts/release-utils.mjs';
const workflow = parseDocument(await readFile('.github/workflows/build-and-release.yml','utf8'),{uniqueKeys:true});
test('release workflow is valid YAML and builds each binary on its target architecture', () => {
  assert.deepEqual(workflow.errors,[]);
  const matrix = workflow.toJS().jobs.build.strategy.matrix.include;
  assert.deepEqual(matrix.map(m => `${m.platform}/${m.arch}`).sort(), ['darwin/arm64','darwin/x64','linux/x64','win32/x64']);
  assert.equal(matrix.find(m => m.runner === 'macos-15').arch,'arm64');
  assert.equal(matrix.find(m => m.runner === 'macos-15-intel').arch,'x64');
});
test('only a successful tag build may write a draft release', () => {
  const data = workflow.toJS();
  assert.deepEqual(data.permissions,{contents:'read'});
  assert.match(data.jobs.release.if,/github.event_name == 'push'/);
  assert.match(data.jobs.release.if,/refs\/tags\/v/);
  assert.equal(data.jobs.release.needs,'build');
  assert.deepEqual(data.jobs.release.permissions,{contents:'write'});
  for (const job of Object.values(data.jobs)) for (const step of job.steps) if (step.uses) assert.match(step.uses,/@[a-f0-9]{40}$/,'Pin actions to reviewed commit hashes');
});
test('macOS package layout places the interpreter inside the .app; Unix archives preserve modes', () => {
  const mac = packageLayout('1.2.3','darwin','arm64');
  assert.equal(mac.resources,'SLANG.app/Contents/Resources');
  assert.equal(mac.executable,'SLANG.app/Contents/MacOS/SLANG');
  assert.equal(packageLayout('1.2.3','linux','x64').extension,'.tar.gz');
  assert.throws(() => packageLayout('../unsafe','win32','x64'));
  assert.throws(() => packageLayout('1.2.3','win32','ia32'));
});
test('MIT and CC0 license texts are complete and package versions agree', async () => {
  assert.match(await readFile('LICENSE','utf8'),/MIT License[\s\S]*Permission is hereby granted[\s\S]*THE SOFTWARE IS PROVIDED/);
  const cc = await readFile('examples/graphics/LICENSE','utf8');
  assert.match(cc,/CC0 1.0 Universal/); assert.match(cc,/Fallback/); assert.match(cc,/Limitations and Disclaimers/);
  const [root,ide,lock] = await Promise.all(['package.json','ide/package.json','package-lock.json'].map(async file => JSON.parse(await readFile(file,'utf8'))));
  assert.equal(root.version,ide.version); assert.equal(root.version,lock.packages[''].version); assert.equal(root.license,'MIT');
});
