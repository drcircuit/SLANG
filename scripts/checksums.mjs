import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { readdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
const directory = path.resolve(process.argv[2] || process.env.SLANG_DIST_DIR || 'dist');
const {version} = JSON.parse(await readFile('package.json','utf8'));
const files = (await readdir(directory)).filter(file => /\.(zip|tar\.gz)$/.test(file) && file.includes(`-${version}-`)).sort();
if (!files.length) throw new Error(`No release archives in ${directory}`);
const lines = [];
for (const file of files) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(path.join(directory,file))) hash.update(chunk);
  lines.push(`${hash.digest('hex')}  ${file}`);
}
await writeFile(path.join(directory,'SHA256SUMS.txt'),lines.join('\n')+'\n');
console.log(`Wrote ${files.length} archive checksums.`);
