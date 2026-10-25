import { _electron as electron } from 'playwright-core';
import { electronTestOptions } from './electron-test-options.mjs';
import { mkdtemp } from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';

const profile = await mkdtemp(path.resolve('build/color-ui-'));
let app;
try {
  app = await electron.launch(await electronTestOptions(profile));
  const page = await app.firstWindow();
  await page.waitForFunction(() => document.querySelector('#tabs').textContent.includes('welcome.slang'));
  const replace = async source => {
    await page.locator('#editor .view-lines').click();
    await page.keyboard.press('ControlOrMeta+a');
    await page.keyboard.insertText(source);
  };
  for (const locale of ['no', 'en']) {
    await page.locator('#language-locale').selectOption(locale);
    await replace('bruk grafikk\ngrafikk.fyll<');
    await page.keyboard.type('"');
    await page.waitForSelector('.suggest-widget.visible');
    const suggestions = await page.locator('.suggest-widget.visible').innerText();
    assert.match(suggestions, locale === 'no' ? /hvit/ : /white/);
    assert.doesNotMatch(suggestions, locale === 'no' ? /white/ : /hvit/);
    await page.keyboard.type(locale === 'no' ? 'li' : 'pu');
    await page.keyboard.press('Enter');
    assert.match(await page.locator('#editor .view-lines').innerText(), locale === 'no' ? /"lilla/ : /"purple/);
    await page.keyboard.press('Escape');
  }
  await page.locator('#language-locale').selectOption('no');
  await replace('bruk grafikk\ngrafikk.fyll<"lilal">');
  for (let i = 0; i < 5; i++) await page.keyboard.press('ArrowLeft');
  await page.keyboard.press('Control+Space');
  await page.waitForSelector('.suggest-widget.visible');
  await page.keyboard.press('Enter');
  assert.match(await page.locator('#editor .view-lines').innerText(), /"lilla">/);
  await replace('bruk grafikk\ngrafikk.bakgrunn<"HVIT">\ngrafikk.fyll<"LILLA">\ngrafikk.sirkel<40, 40, 20>');
  await page.locator('#run').click();
  await page.waitForSelector('#canvas.visible');
  await page.waitForFunction(() => document.querySelector('#canvas').getContext('2d').getImageData(40,40,1,1).data[0] === 128);
  const pixels = await page.evaluate(() => {
    const ctx = document.querySelector('#canvas').getContext('2d');
    return [[0,0], [40,40]].map(([x,y]) => [...ctx.getImageData(x,y,1,1).data]);
  });
  assert.deepEqual(pixels, [[255,255,255,255], [128,0,128,255]]);
  await page.waitForFunction(() => !document.querySelector('#debug').disabled);
  console.log('Color UI passed: automatic suggestions, language filtering, insertion and native canvas pixels.');
} finally {
  if (app) {
    await app.evaluate(({BrowserWindow}) => { for (const window of BrowserWindow.getAllWindows()) window.destroy(); }).catch(() => {});
    await app.close();
  }
}
