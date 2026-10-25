// Optional independent arithmetic oracle. Uses Node BigInt only in development;
// SLANG's packaged interpreter has no Node dependency.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';

const executable = process.argv[2];
if (!executable) throw new Error('Usage: node scripts/verify-arithmetic.mjs <slang executable>');
let seed = 0x516a9;
const random = max => {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  return seed % max;
};
const power = places => 10n ** BigInt(places);
function format(coefficient, scale) {
  const sign = coefficient < 0n ? '-' : '';
  let digits = (coefficient < 0n ? -coefficient : coefficient).toString();
  if (scale) {
    digits = digits.padStart(scale + 1, '0');
    digits = digits.slice(0, -scale) + '.' + digits.slice(-scale);
    digits = digits.replace(/0+$/, '').replace(/\.$/, '');
  }
  return coefficient === 0n ? '0' : sign + digits;
}
function sample() {
  let digits = String(random(9) + 1);
  for (let i = 0, n = random(80); i < n; ++i) digits += random(10);
  return { coefficient: BigInt(digits) * (random(2) ? 1n : -1n), scale: random(25) };
}
function quotient(numerator, denominator, places) {
  const negative = (numerator < 0n) !== (denominator < 0n);
  numerator = numerator < 0n ? -numerator : numerator;
  denominator = denominator < 0n ? -denominator : denominator;
  let q = numerator / denominator;
  const rest = numerator % denominator;
  if (rest * 2n > denominator || (rest * 2n === denominator && q % 2n)) ++q;
  return format(negative ? -q : q, places);
}
// Binary search is deliberately independent of the interpreter's Newton method.
function squareRoot(coefficient, scale, places) {
  const numerator = coefficient * power(2 * places), denominator = power(scale);
  const integer = numerator / denominator;
  let low = 0n, high = integer + 1n;
  while (low + 1n < high) {
    const middle = (low + high) / 2n;
    if (middle * middle <= integer) low = middle; else high = middle;
  }
  const midpoint = (2n * low + 1n) ** 2n * denominator;
  if (4n * numerator > midpoint || (4n * numerator === midpoint && low % 2n)) ++low;
  return format(low, places);
}

let checked = 0;
for (let batch = 0; batch < 60; ++batch) {
  const source = ['use numbers as n'], expected = [];
  const check = (expression, result) => { source.push('print<(' + expression + ')>'); expected.push(result); };
  const a = sample(), b = sample();
  const left = '(' + format(a.coefficient, a.scale) + ')';
  const right = '(' + format(b.coefficient, b.scale) + ')';
  const scale = Math.max(a.scale, b.scale);
  const x = a.coefficient * power(scale - a.scale), y = b.coefficient * power(scale - b.scale);
  check(left + ' + ' + right, format(x + y, scale));
  check(left + ' - ' + right, format(x - y, scale));
  check(left + ' * ' + right, format(a.coefficient * b.coefficient, a.scale + b.scale));
  const places = random(35);
  check('divide<' + left + ', ' + right + ', ' + places + '>', quotient(a.coefficient * power(b.scale + places), b.coefficient * power(a.scale), places));
  check(left + ' / ' + right, quotient(a.coefficient * power(b.scale + 28), b.coefficient * power(a.scale), 28));
  check('round<' + left + ', ' + places + '>', a.scale <= places ? format(a.coefficient, a.scale) : quotient(a.coefficient, power(a.scale - places), places));
  const whole = a.coefficient / power(a.scale), remainder = a.coefficient % power(a.scale);
  check('n.floor<' + left + '>', String(whole - (remainder < 0n ? 1n : 0n)));
  check('n.ceil<' + left + '>', String(whole + (remainder > 0n ? 1n : 0n)));
  const exponent = random(6);
  check('n.pow<' + left + ', ' + exponent + '>', format(a.coefficient ** BigInt(exponent), a.scale * exponent));
  const positive = a.coefficient < 0n ? -a.coefficient : a.coefficient;
  check('n.sqrt<' + format(positive, a.scale) + ', ' + places + '>', squareRoot(positive, a.scale, places));
  const result = spawnSync(executable, ['run', '-'], { input: source.join('\n'), encoding: 'utf8', timeout: 10000 });
  assert.equal(result.status, 0, result.stderr || String(result.error));
  assert.deepEqual(result.stdout.trim().split(/\r?\n/), expected, 'numeric oracle mismatch in batch ' + batch);
  checked += expected.length;
}
console.log(checked + ' large integer/decimal results match the independent BigInt oracle.');
