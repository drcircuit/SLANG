import test from 'node:test';
import assert from 'node:assert/strict';
import { palettes, normalizeTheme, createTheme } from '../ide/themes.mjs';
const luminance = hex => [1,3,5].map(i => parseInt(hex.slice(i,i+2),16)/255).map(n => n <= .04045 ? n/12.92 : ((n+.055)/1.055)**2.4).reduce((sum,n,i) => sum + n * [.2126,.7152,.0722][i],0);
const contrast = (a,b) => (Math.max(luminance(a),luminance(b))+.05)/(Math.min(luminance(a),luminance(b))+.05);
test('theme preferences recover from invalid or obsolete saved values', () => {
  assert.deepEqual(normalizeTheme(null, true), { mode: 'dark', palette: 'teal' });
  assert.deepEqual(normalizeTheme({ mode: 'invalid', palette: '__proto__' }), { mode: 'light', palette: 'teal' });
});
for (const mode of ['light','dark']) for (const palette of Object.keys(palettes)) {
  test(`${mode} ${palette}: text, syntax and focus remain readable`, () => {
    const theme = createTheme({ mode, palette }), t = theme.tokens, minimum = theme.highContrast ? 7 : 4.5;
    for (const background of [t.surface,t.panel,t.background,t.selected]) for (const text of [t.text,t.muted]) assert.ok(contrast(text,background) >= minimum, `${text} on ${background}`);
    for (const rule of theme.monaco.rules) assert.ok(contrast('#' + rule.foreground,t.surface) >= minimum, `Syntax: ${rule.token}`);
    assert.ok(contrast(t['on-accent'],t.accent) >= minimum, 'Run button label');
    assert.ok(contrast(t.accent,t.panel) >= 3, 'Focus ring');
    if (theme.highContrast) assert.ok(contrast(t.border,t.surface) >= 7, 'High contrast boundaries');
  });
}
