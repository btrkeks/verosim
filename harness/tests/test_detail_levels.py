"""Pin DETAIL_LEVELS (D14) against the vendored musicdiff enum.

Catches silent drift if the vendored musicdiff DetailLevel bit assignments
ever change. Data-free, but needs the harness venv (imports musicdiff).
"""

import unittest

from verosim_harness import DETAIL_LEVELS


class DetailLevels(unittest.TestCase):
    def test_values_match_vendored_enum(self):
        from musicdiff import DetailLevel as D

        tier_a = D.NotesAndRests | D.Beams | D.Signatures
        tier_ab = tier_a | D.Ties | D.Slurs | D.Articulations
        self.assertEqual(DETAIL_LEVELS["tierA"], int(tier_a))
        self.assertEqual(DETAIL_LEVELS["tierAB"], int(tier_ab))
        self.assertEqual(DETAIL_LEVELS["tierAB_dir"], int(tier_ab | D.Directions))

    def test_lyrics_never_included(self):
        from musicdiff import DetailLevel as D

        for name, value in DETAIL_LEVELS.items():
            self.assertFalse(value & D.Lyrics, f"{name} includes Lyrics (excluded in v1)")


if __name__ == "__main__":
    unittest.main()
