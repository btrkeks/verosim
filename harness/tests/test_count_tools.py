"""Count tooling tests: count_audit join logic + count_oracle golden.

The golden values are the HAND-10 hand counts (corpora/hand10/expected/),
which were cross-checked three ways (hand / count_oracle / C++
--count-symbols). If the vendored musicdiff or pinned music21 stack drifts,
the golden test catches it here, on committed data.
"""

import json
import unittest
from pathlib import Path

from verosim_harness.count_audit import join_audit, summarize

REPO = Path(__file__).resolve().parents[2]


class TestJoinAudit(unittest.TestCase):
    def _pair(self, pred="a.xml", gt="a.krn", n_pred=100, n_gt=110):
        return {"pair": {"pred": pred, "gt": gt}, "n_pred": n_pred, "n_gt": n_gt}

    def test_join_and_deltas(self):
        counts = {
            "a.xml": {"ok": True, "total": 98, "categories": {"pitches": 50}, "warnings": []},
            "a.krn": {"ok": True, "total": 110, "categories": {"pitches": 55}, "warnings": []},
        }
        rows = join_audit([self._pair()], counts)
        self.assertEqual(len(rows), 2)
        pred = next(r for r in rows if r["side"] == "pred")
        gt = next(r for r in rows if r["side"] == "gt")
        self.assertEqual(pred["delta"], -2)
        self.assertEqual(gt["delta"], 0)
        self.assertIsNone(pred["error"])

    def test_missing_and_failed_cpp_records(self):
        counts = {"a.xml": {"ok": False, "error": "load failed"}}
        rows = join_audit([self._pair()], counts)
        pred = next(r for r in rows if r["side"] == "pred")
        gt = next(r for r in rows if r["side"] == "gt")
        self.assertEqual(pred["error"], "load failed")
        self.assertEqual(gt["error"], "no_cpp_record")
        self.assertIsNone(pred["delta"])

    def test_summary_percentages(self):
        counts = {
            "a.xml": {"ok": True, "total": 98, "categories": {}, "warnings": []},
            "a.krn": {"ok": True, "total": 110, "categories": {}, "warnings": []},
        }
        summary = summarize(join_audit([self._pair()], counts))
        self.assertEqual(summary["sides"]["pred"]["sum_abs_delta"], 2)
        self.assertEqual(summary["sides"]["pred"]["abs_delta_pct"], 2.0)
        self.assertEqual(summary["sides"]["gt"]["exact_matches"], 1)
        self.assertEqual(summary["sides"]["gt"]["signed_delta_pct"], 0.0)


class TestCountOracleGolden(unittest.TestCase):
    """count_oracle must keep reproducing the HAND-10 expected counts."""

    def test_hand10_expected_files_reproduce(self):
        from verosim_harness.count_oracle import count_file

        hand10 = REPO / "corpora" / "hand10"
        expected_files = sorted((hand10 / "expected").glob("*.expected.json"))
        self.assertEqual(len(expected_files), 10)
        # two representative files keep the test fast; the full corpus is
        # gated exactly in C++ (tests/test_hand10.cpp) on every make test
        for name in ("mono", "tiny"):
            with self.subTest(name=name):
                expected = json.loads((hand10 / "expected" / f"{name}.expected.json").read_text())
                got = count_file(hand10 / expected["file"], "tierA")
                self.assertEqual(got["total"], expected["total"])
                for key, value in expected["categories"].items():
                    self.assertEqual(got["categories"][key], value, f"category {key}")


if __name__ == "__main__":
    unittest.main()
