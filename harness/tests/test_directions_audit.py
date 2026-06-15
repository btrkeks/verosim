import unittest

from verosim_harness.directions_audit import classify_extra_kind, extra_kind_from_repr


class DirectionsAudit(unittest.TestCase):
    def test_extra_kind_from_oracle_repr(self):
        self.assertEqual(
            extra_kind_from_repr("Extra(139818106332672):dynamic,symbol=pp,off=0.0"),
            "dynamic",
        )
        self.assertEqual(
            extra_kind_from_repr("Extra(140716563967696):direction,content=rall.,off=2.0"),
            "direction",
        )

    def test_classify_extra_kind_uses_available_side(self):
        op = {
            "op": "extradel",
            "a": {
                "type": "AnnExtra",
                "repr": "Extra(1):diminuendo,off=1.0,dur=1.5",
            },
            "b": None,
        }
        self.assertEqual(classify_extra_kind(op), "diminuendo")

        op = {
            "op": "extrains",
            "a": None,
            "b": {
                "type": "AnnExtra",
                "repr": "Extra(2):pedalmark,off=0.0,dur=4.0",
            },
        }
        self.assertEqual(classify_extra_kind(op), "pedalmark")


if __name__ == "__main__":
    unittest.main()
