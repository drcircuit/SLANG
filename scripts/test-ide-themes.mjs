import { electronTestOptions } from './electron-test-options.mjs';
import { _electron as electron } from 'playwright-core';
import { mkdir } from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import { palettes, createTheme } from '../ide/themes.mjs';
const directory = path.resolve('build/ide-themes'); await mkdir(directory, { recursive: true });
let application;
const errors = [];
async function launch() {
  application = await electron.launch(await electronTestOptions(path.join(directory,'profile')));
  const page = await application.firstWindow();
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', event => { if (event.type() === 'error') errors.push(event.text()); });
  await page.waitForFunction(() => document.querySelector('#tabs')?.textContent.includes('welcome.slang'));
  return page;
}
async function close() {
  await application.evaluate(({ BrowserWindow }) => { for (const window of BrowserWindow.getAllWindows()) window.destroy(); });
  await application.close(); application = null;
}
try {
  const page = await launch();
  await page.locator('#drawing-example').click(); await page.locator('#run').click();
  await page.waitForFunction(() => document.querySelector('#console').textContent.includes('Program finished.'));
  const rgb = hex => `rgb(${[1,3,5].map(i => parseInt(hex.slice(i,i+2),16)).join(', ')})`;
  for (const mode of ['light','dark']) for (const palette of Object.keys(palettes)) {
    await page.locator('#theme-mode').selectOption(mode); await page.locator('#theme-palette').selectOption(palette);
    const expected = createTheme({ mode, palette });
    const actual = await page.evaluate(() => ({
      mode: document.documentElement.dataset.mode, palette: document.documentElement.dataset.palette,
      editor: getComputedStyle(document.querySelector('.monaco-editor')).backgroundColor,
      console: getComputedStyle(document.querySelector('.console-section')).backgroundColor,
      toolbar: getComputedStyle(document.querySelector('.toolbar')).backgroundColor,
      pixel: [...document.querySelector('#canvas').getContext('2d').getImageData(0,0,1,1).data],
    }));
    assert.equal(actual.mode, mode); assert.equal(actual.palette, palette);
    for (const panel of ['editor','console','toolbar']) assert.equal(actual[panel], rgb(expected.tokens.surface), panel);
    assert.deepEqual(actual.pixel, [24,36,58,255], 'Appearance must not recolor a program drawing');
    if ((mode === 'dark' && ['purple','contrast'].includes(palette)) || (mode === 'light' && ['pink','contrast'].includes(palette))) await page.screenshot({ path: path.join(directory,`${mode}-${palette}.png`) });
  }
  await application.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows()[0].setSize(960, 640));
  assert.ok(await page.locator('.appearance').evaluate(element => element.getBoundingClientRect().right <= innerWidth));
  assert.ok(await page.locator('#step').evaluate(element => element.getBoundingClientRect().right < document.querySelector('.appearance').getBoundingClientRect().left));
  await page.screenshot({ path: path.join(directory,'compact.png') });
  await page.locator('#theme-mode').selectOption('dark'); await page.locator('#theme-palette').selectOption('purple');
  await close();
  const reopened = await launch();
  assert.equal(await reopened.locator('#theme-mode').inputValue(), 'dark'); assert.equal(await reopened.locator('#theme-palette').inputValue(), 'purple');
  // Keyboard navigation through native select controls also updates the theme.
  await reopened.locator('#theme-mode').focus(); await reopened.keyboard.press('ArrowUp'); await reopened.keyboard.press('Enter');
  assert.equal(await reopened.locator('#theme-mode').inputValue(), 'light');
  assert.deepEqual(errors, []);
  console.log('All 16 appearance combinations passed: editor/chrome colors, canvas preservation, compact layout, keyboard selection, and persistence across restart.');
} finally { if (application) await close(); }
