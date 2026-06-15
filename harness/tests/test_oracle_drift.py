"""Drift tripwire: run_pair must agree with musicdiff._diff_omr_ned_metrics.

oracle.run_pair mirrors the vendored _diff_omr_ned_metrics recipe line for
line (it exists only because upstream discards op_list, which we need as
edit_ops). Per D12 the vendored code is the spec, so any numeric disagreement
between the mirror and the real entry point is a harness bug — typically a
musicdiff submodule bump that changed the recipe without this mirror being
re-synced. The single-pair golden test cannot catch recipe changes that
happen not to affect its one score; this sweep over the mutation corpus
bases/cases (tiny, in-repo, fast to parse) casts a wider net.
"""

import unittest
from pathlib import Path

from verosim_harness import DETAIL_LEVELS

MUTATIONS = Path(__file__).resolve().parents[2] / "corpora" / "mutations"

# (pred, gt) — mirrors the corpus orientation: pred=mutated, gt=base.
# One combo per base file, plus one identity pair.
PAIRS = [
    ("base/mono.krn", "base/mono.krn"),  # identity: distance must be 0
    ("cases/mono_pitch_letter.krn", "base/mono.krn"),
    ("cases/chords_dot_shift.krn", "base/chords.krn"),
    ("cases/keysig_to_flats.krn", "base/keysig.krn"),
    ("cases/tuplet_pitch.krn", "base/tuplet.krn"),
    ("cases/grand_bass_delete.krn", "base/grand.krn"),
    ("cases/accid_flat_drop.krn", "base/accid.krn"),
]


class OracleDrift(unittest.TestCase):
    def test_run_pair_matches_vendored_entry_point(self):
        from verosim_harness.oracle import ensure_converter21, run_pair

        ensure_converter21()
        from musicdiff import _diff_omr_ned_metrics

        for detail_name in sorted(DETAIL_LEVELS):
            for pred, gt in PAIRS:
                with self.subTest(detail=detail_name, pred=pred, gt=gt):
                    record = run_pair(pred, gt, detail_name, MUTATIONS)
                    self.assertIsNone(record["error"])

                    # the signature accepts DetailLevel | int
                    metrics = _diff_omr_ned_metrics(
                        MUTATIONS / pred, MUTATIONS / gt, DETAIL_LEVELS[detail_name]
                    )
                    self.assertIsNotNone(metrics)
                    self.assertEqual(record["distance"], metrics.omr_edit_distance)
                    self.assertEqual(record["n_pred"], metrics.pred_numsyms)
                    self.assertEqual(record["n_gt"], metrics.gt_numsyms)
                    self.assertEqual(record["omr_ned"], metrics.omr_ned)
                    self.assertEqual(
                        record["edit_distances_dict"], metrics.edit_distances_dict
                    )
                    # edit_ops is the one thing the mirror adds. Its costs do
                    # NOT sum to the distance in general: syntax-fix costs
                    # (acceptSyntaxErrors repairs, charged inside
                    # annotated_scores_diff) are part of the distance and of
                    # the category dict, but are not ops in op_list. The
                    # invariants are: categories account for the full
                    # distance, ops for everything except syntax fixes.
                    syntax_cost = record["edit_distances_dict"].get(
                        "bad kern syntax OMR-ED", 0
                    )
                    self.assertEqual(
                        sum(record["edit_distances_dict"].values()),
                        record["distance"],
                    )
                    self.assertEqual(
                        sum(op["cost"] for op in record["edit_ops"]),
                        record["distance"] - syntax_cost,
                    )

    def test_identity_pair_is_zero(self):
        from verosim_harness.oracle import run_pair

        record = run_pair("base/mono.krn", "base/mono.krn", "tierA", MUTATIONS)
        self.assertIsNone(record["error"])
        self.assertEqual(record["distance"], 0)
        self.assertEqual(record["omr_ned"], 0.0)


if __name__ == "__main__":
    unittest.main()
