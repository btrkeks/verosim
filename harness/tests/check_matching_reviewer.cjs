// Run against a generated review.html; use a fresh browser so human labels stay intact.
// PLAYWRIGHT_MODULE can name an installed @playwright/test module outside this repo.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || "@playwright/test");
const assert = require("node:assert/strict");
const { pathToFileURL } = require("node:url");
const path = require("node:path");

async function check() {
  const html = process.argv[2];
  if (!html) throw new Error("Usage: node check_matching_reviewer.cjs /path/to/review.html [screenshot.png]");
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1720, height: 1100 } });
    const errors = [];
    page.on("pageerror", (error) => errors.push(String(error)));
    await page.goto(pathToFileURL(path.resolve(html)).href);
    await page.waitForFunction(() => window.matchingReview);
    const windows = await page.evaluate(() => window.matchingReview.bundle.windows.length);
    let checkedLinks = 0;
    for (let index = 0; index < windows; index++) {
      await page.selectOption("#cases", String(index));
      await page.click('[data-level="measure"]');
      const hiddenBars = await page.evaluate(() =>
        [...document.querySelectorAll("[data-measure-key]")].filter((el) => {
          const box = el.getBoundingClientRect();
          return !box.width || !box.height;
        }).map((el) => el.dataset.measureKey),
      );
      assert.deepEqual(hiddenBars, [], `Hidden bars in window ${index}`);
      const visibleNotes = () => page.evaluate(() =>
        [...document.querySelectorAll(".score-body .note")].filter((el) => {
          const box = el.getBoundingClientRect();
          return box.width && box.height;
        }).map((el) => el.closest(".score-body").id + "/" + el.id),
      );
      const plainNotes = await visibleNotes();
      await page.check("#current-marks");
      assert.deepEqual(await visibleNotes(), plainNotes, "Edit colors changed which notes are visible");
      await page.uncheck("#current-marks");
      assert.deepEqual(await visibleNotes(), plainNotes, "Hiding edit colors removed notes");
      const links = await page.evaluate(() => window.matchingReview.labels.windows.find(
        (w) => w.id === window.matchingReview.current.id,
      ).levels.measure.links);
      for (const link of links) {
        await page.selectOption("#ground_truth-pick", []);
        await page.selectOption("#prediction-pick", link.prediction);
        for (const key of link.ground_truth) {
          const partner = page.locator(`#ground_truth-score [data-measure-key="${key}"].rv-linked`);
          assert(await partner.isVisible(), `Invisible proposed partner ${key} in window ${index}`);
          const color = await partner.locator(".rv-bar-hitbox").evaluate((el) => getComputedStyle(el).stroke);
          assert.equal(color, "rgb(19, 116, 81)", "Proposed partner is not painted green");
        }
        checkedLinks++;
      }
    }
    // Reproduce the user's exact selection, including a reference bar marked inserted.
    await page.selectOption("#cases", "0");
    await page.selectOption("#prediction-pick", "p0/m14");
    assert((await page.locator("#selection-summary").innerText()).includes("Reference: Staff 1, bar 14"));
    const partner = page.locator('#ground_truth-score [data-measure-key="p0/m14"]');
    assert(await partner.isVisible());
    assert((await partner.locator(".rv-bar-label").textContent()).includes("Bar 14"));
    await page.check("#current-marks");
    assert.equal(await partner.locator("use").first().evaluate((el) => getComputedStyle(el).fill), "rgb(19, 116, 81)");
    await page.uncheck("#current-marks");
    if (process.argv[3]) await page.screenshot({ path: process.argv[3], fullPage: true });
    await page.click("#clear-selection");
    await page.locator('#prediction-score [data-measure-key="p0/m14"] .rv-bar-hitbox').click();
    assert(await page.locator('#ground_truth-score [data-measure-key="p0/m14"].rv-linked').isVisible());
    await page.click("#clear-selection");
    await page.selectOption("#ground_truth-pick", "p0/m15");
    assert((await page.locator("#selection-summary").innerText()).includes("No proposed partner"));
    assert.equal(await page.locator(".rv-linked").count(), 0);
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({ windows, checkedLinks, checks: ["all scoped bars visible", "edit toggle preserves visible notes", "proposed partners visible and green", "selected pair named", "bar labels visible", "score click selects partner", "unmatched selection explained"], errors }, null, 2));
  } finally {
    await browser.close();
  }
}

check().catch((error) => { console.error(error); process.exitCode = 1; });
