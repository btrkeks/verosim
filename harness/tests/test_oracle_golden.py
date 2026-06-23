"""Golden test: run_pair emits the active-mode oracle schema and costs."""

import unittest
from pathlib import Path

MUTATIONS = Path(__file__).resolve().parents[2] / "corpora" / "mutations"


class OracleGolden(unittest.TestCase):
    def test_oracle_active_articulation_case(self):
        from verosim_harness.oracle import run_pair

        r = run_pair("cases/mono_artic_staccato.krn", "base/mono.krn", "active", MUTATIONS)
        self.assertIsNone(r["error"])
        self.assertEqual(r["mode"], {"name": "active", "value": 755})
        self.assertEqual(r["distance"], 1)
        self.assertEqual(
            r["edit_distances_dict"],
            {"wrong articulation OMR-ED": 1, "bad kern syntax OMR-ED": 0},
        )
        self.assertEqual(
            {op["op"]: op["cost"] for op in r["edit_ops"]},
            {"delarticulation": 1},
        )


if __name__ == "__main__":
    unittest.main()
