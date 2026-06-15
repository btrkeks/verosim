"""Data-free CI checks on the committed corpus lists."""

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CORPORA = REPO_ROOT / "corpora"


def read_rows(name: str) -> list[str]:
    return [
        line
        for line in (CORPORA / name).read_text(encoding="utf-8").splitlines()
        if line and not line.startswith("#")
    ]


class CorporaInvariants(unittest.TestCase):
    def test_dev200(self):
        rows = read_rows("dev200.tsv")
        self.assertEqual(len(rows), 200)
        self.assertEqual(len(set(rows)), 200)
        for row in rows:
            pred, gt = row.split("\t")
            self.assertTrue(pred.endswith(".xml") and "0_raw_xml" in pred, pred)
            self.assertTrue(gt.endswith(".krn") and "1_kern_conversions" in gt, gt)
            self.assertEqual(Path(pred).stem, Path(gt).stem)

    def test_holdout100(self):
        rows = read_rows("holdout100.tsv")
        self.assertEqual(len(rows), 169)  # 100 lieder + 69 musetrainer
        self.assertEqual(len(set(rows)), 169)
        lieder = [r for r in rows if "openscore-lieder" in r]
        musetrainer = [r for r in rows if "musetrainer" in r]
        self.assertEqual(len(lieder), 100)
        self.assertEqual(len(musetrainer), 69)

    def test_dev_holdout_disjoint(self):
        self.assertFalse(set(read_rows("dev200.tsv")) & set(read_rows("holdout100.tsv")))

    def test_perf10k(self):
        rows = read_rows("perf10k.txt")
        self.assertEqual(len(rows), 10_000)
        self.assertEqual(len(set(rows)), 10_000)
        self.assertTrue(all(r.endswith(".krn") and "1_kern_conversions" in r for r in rows))
        by_ds = {ds: sum(1 for r in rows if f"train/{ds}/" in r) for ds in ("pdmx", "grandstaff")}
        self.assertEqual(sum(by_ds.values()), 10_000)
        # proportional split over 234,461 + 41,598 source files
        self.assertEqual(by_ds["pdmx"], 8493)
        self.assertEqual(by_ds["grandstaff"], 1507)

    def test_lieder_all(self):
        rows = read_rows("lieder_all_krn.txt")
        self.assertEqual(len(rows), 1462)
        self.assertEqual(len(set(rows)), 1462)


if __name__ == "__main__":
    unittest.main()
