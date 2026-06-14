import unittest
from pathlib import Path

from verosim_harness.corr_audit import detail_from_oracle_path


class CorrAuditTests(unittest.TestCase):
    def test_detail_from_oracle_path_handles_holdout_names(self):
        self.assertEqual(detail_from_oracle_path(Path("oracle/holdout100_tierAB.jsonl")), "tierAB")
        self.assertEqual(detail_from_oracle_path(Path("oracle/dev200_tierAB_dir.jsonl")), "tierAB_dir")

    def test_detail_from_oracle_path_rejects_ambiguous_names(self):
        with self.assertRaises(ValueError):
            detail_from_oracle_path(Path("oracle/holdout100.jsonl"))


if __name__ == "__main__":
    unittest.main()
