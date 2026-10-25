import { readFile } from 'node:fs/promises';
export async function checkRelease() {
  const read = async file => JSON.parse(await readFile(file,'utf8'));
  const pkg = await read('package.json'), ide = await read('ide/package.json'), lock = await read('package-lock.json');
  if (![ide.version,lock.version,lock.packages[''].version].every(v => v === pkg.version)) throw new Error('Release versions differ. Run node scripts/set-version.mjs <version>.');
  if (!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(pkg.version)) throw new Error('Invalid release version.');
  if (process.env.GITHUB_REF_TYPE === 'tag' && process.env.GITHUB_REF_NAME !== `v${pkg.version}`) throw new Error(`Tag must be v${pkg.version}, matching package.json.`);
  for (const file of ['LICENSE','THIRD-PARTY-NOTICES.md','examples/graphics/LICENSE','native/assets/OFL.txt','native/vendor/stb/LICENSE']) if (!(await readFile(file,'utf8')).trim()) throw new Error(`Missing license: ${file}`);
  return pkg;
}
const pkg = await checkRelease(); console.log(`Release metadata and licenses OK: ${pkg.version}`);
