import { readFileSync } from 'node:fs';
import { parse } from './parser.js';

const [command, ...paths] = process.argv.slice(2);
if (!['check', 'ast'].includes(command) || paths.length === 0 || (command === 'ast' && paths.length !== 1)) {
  console.error('Usage: node src/cli.js check <files...> | ast <file>');
  process.exitCode = 2;
} else {
  for (const path of paths) {
    try {
      const ast = parse(readFileSync(path, 'utf8'));
      if (command === 'ast') console.log(JSON.stringify(ast, null, 2));
      else console.log(`${path}: syntax OK (not type-checked or executed)`);
    } catch (error) {
      console.error(`${path}: ${error.message}`);
      process.exitCode = 1;
    }
  }
}
