"""Pin METRIC_MODES against the vendored musicdiff enum.

Catches silent drift if the vendored musicdiff DetailLevel bit assignments
ever change. Data-free, but needs the harness venv (imports musicdiff).
"""

import unittest

from verosim_harness import METRIC_MODES


class MetricModes(unittest.TestCase):
    def test_values_match_vendored_enum(self):
        from musicdiff import DetailLevel as D

        active = (
            D.NotesAndRests | D.Beams | D.Signatures
            | D.Ties | D.Slurs | D.Articulations | D.Barlines
        )
        self.assertEqual(METRIC_MODES["active"], int(active))
        self.assertEqual(METRIC_MODES["experimental"], int(active | D.Directions | D.Ottavas))

    def test_lyrics_never_included(self):
        from musicdiff import DetailLevel as D

        for name, value in METRIC_MODES.items():
            self.assertFalse(value & D.Lyrics, f"{name} includes Lyrics (excluded in v1)")


if __name__ == "__main__":
    unittest.main()
