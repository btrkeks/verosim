import unittest
from pathlib import Path

from verosim_harness.corr_audit import mode_from_oracle_path


class CorrAuditTests(unittest.TestCase):
    def test_mode_from_oracle_path_handles_holdout_names(self):
        self.assertEqual(mode_from_oracle_path(Path("oracle/holdout100_active.jsonl")), "active")
        self.assertEqual(mode_from_oracle_path(Path("oracle/dev200_experimental.jsonl")), "experimental")

    def test_mode_from_oracle_path_rejects_ambiguous_names(self):
        with self.assertRaises(ValueError):
            mode_from_oracle_path(Path("oracle/holdout100.jsonl"))


if __name__ == "__main__":
    unittest.main()
