import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
const require = createRequire(import.meta.url);
export async function electronTestOptions(profile) {
  let executable = process.env.SLANG_IDE_EXECUTABLE;
  if (process.env.SLANG_TEST_PACKAGED) executable = JSON.parse(await readFile('build/ide-package.json','utf8')).executable;
  const environment = { ...process.env, SLANG_IDE_PROFILE: profile };
  if (executable) delete environment.SLANG_BINARY;
  if (process.env.SLANG_IDE_MINIMAL_PATH) {
    for (const name of Object.keys(environment)) if (name.toLowerCase() === 'path') delete environment[name];
    environment.PATH = process.platform === 'win32' ? path.join(process.env.SystemRoot,'System32') : '/usr/bin:/bin';
  }
  return { executablePath: executable || require('electron'), args: executable ? [] : [path.resolve('ide')], env: environment, timeout: 30000 };
}
