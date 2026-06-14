import unittest

from verosim_harness.synth_corr import format_rho, spearman_or_none


class SynthCorrTests(unittest.TestCase):
    def test_spearman_or_none_requires_enough_nonconstant_rows(self):
        self.assertIsNone(spearman_or_none([]))
        self.assertIsNone(spearman_or_none([{"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.1}]))
        self.assertIsNone(
            spearman_or_none(
                [
                    {"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.1},
                    {"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.2},
                ]
            )
        )

    def test_spearman_or_none_formats_valid_values(self):
        rows = [
            {"cpp_omr_ned": 0.1, "oracle_omr_ned": 0.2},
            {"cpp_omr_ned": 0.2, "oracle_omr_ned": 0.3},
            {"cpp_omr_ned": 0.3, "oracle_omr_ned": 0.4},
        ]
        self.assertEqual(format_rho(spearman_or_none(rows)), "1.0000")
        self.assertEqual(format_rho(None), "n/a")


if __name__ == "__main__":
    unittest.main()
