import tempfile
import unittest
from pathlib import Path

from verosim_harness.gates import RecordCompleteness, clean_failure_audit, threshold_status
from verosim_harness.lists import read_list, read_pairs
from verosim_harness.stats import format_rho, spearman, spearman_or_none


class HarnessHelperTests(unittest.TestCase):
    def test_shared_list_readers_skip_comments_and_blanks(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "pairs.tsv"
            path.write_text("# comment\n\npred.xml\tgt.krn\nnext.xml\tnext.krn\n", encoding="utf-8")

            self.assertEqual(read_list(path), ["pred.xml\tgt.krn", "next.xml\tnext.krn"])
            self.assertEqual(read_pairs(path), [("pred.xml", "gt.krn"), ("next.xml", "next.krn")])

    def test_spearman_handles_ties_and_undefined_values(self):
        self.assertAlmostEqual(spearman([1.0, 2.0, 2.0], [1.0, 2.0, 2.0]), 1.0)
        self.assertIsNone(
            spearman_or_none(
                [
                    {"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.1},
                    {"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.2},
                ]
            )
        )
        self.assertEqual(format_rho(None), "n/a")

    def test_threshold_and_record_completeness_summaries(self):
        self.assertEqual(threshold_status(0.95, 0.9), "PASS")
        self.assertEqual(threshold_status(None, 0.9), "FAIL")

        completeness = RecordCompleteness(expected=3, emitted=2)
        self.assertEqual(completeness.missing, 1)
        self.assertFalse(completeness.ok)

    def test_clean_failure_audit_tracks_missing_and_message_policy(self):
        records = [
            {"path": "recovered.krn", "ok": True},
            {"path": "clean-failure.krn", "ok": False, "errors": ["parse failed"]},
            {"path": "bad-failure.krn", "ok": False, "errors": []},
        ]
        audit = clean_failure_audit(expected=4, records=records)
        self.assertEqual(audit.completeness.missing, 1)
        self.assertEqual(len(audit.recovered_loads), 1)
        self.assertEqual(len(audit.missing_errors), 1)
        self.assertFalse(audit.ok)


if __name__ == "__main__":
    unittest.main()
