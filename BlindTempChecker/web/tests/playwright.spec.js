const { test, expect } = require('@playwright/test');
const path = require('path');

test('UI loads and shows temp element', async ({ page }) => {
  const file = 'file://' + path.resolve(__dirname, '..', 'index.html');
  await page.goto(file);
  const el = await page.locator('#temp');
  await expect(el).toBeVisible();
});
