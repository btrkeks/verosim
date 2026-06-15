"""Golden test: run_pair reproduces the oracle smoke-check values exactly.

Skipped when the Transcoda data root is absent.
"""

import unittest
from pathlib import Path

from verosim_harness import DEFAULT_DATA_ROOT

PAIR = (
    "train/openscore-lieder/0_raw_xml/lc28688206.xml",
    "train/openscore-lieder/1_kern_conversions/lc28688206.krn",
)


@unittest.skipUnless(DEFAULT_DATA_ROOT and Path(DEFAULT_DATA_ROOT).is_dir(), "Transcoda data root not available")
class OracleGolden(unittest.TestCase):
    def test_oracle_smoke_tier_a(self):
        from verosim_harness.oracle import run_pair

        r = run_pair(*PAIR, "tierA", DEFAULT_DATA_ROOT)
        self.assertIsNone(r["error"])
        self.assertEqual(r["distance"], 148)
        self.assertEqual(r["n_pred"], 1095)
        self.assertEqual(r["n_gt"], 1097)
        self.assertAlmostEqual(r["omr_ned"], 0.06751824817518248)
        self.assertEqual(sum(op["cost"] for op in r["edit_ops"]), 148)


if __name__ == "__main__":
    unittest.main()
