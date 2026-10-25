import { readFile, writeFile } from 'node:fs/promises';
const version = process.argv[2];
if (!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version || '')) throw new Error('Usage: node scripts/set-version.mjs 0.6.2 (or 0.6.2-rc.1)');
for (const file of ['package.json','ide/package.json','package-lock.json']) {
  const data = JSON.parse(await readFile(file,'utf8')); data.version = version;
  if (file === 'package-lock.json') data.packages[''].version = version;
  await writeFile(file, JSON.stringify(data,null,2) + '\n');
}
console.log(`Release version set to ${version}. CMake reads this version from package.json.`);
