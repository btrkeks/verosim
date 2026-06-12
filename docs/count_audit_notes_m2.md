## Outlier triage (M2 exit, 2026-06-12)

Every top-20 outlier was fingerprinted by category (note categories vs
signature categories) and traced to one of three importer differences —
all accepted parser differences per D6, none a Layer 2 extraction bug:

1. **converter21 signature restating on rhythm-broken kern** (all 13 kern
   outliers, −405…−1064 each). Files whose kern rhythm humlib also flags
   ("Inconsistent rhythm analysis" on stderr) make converter21 restate
   clef + key signature + time signature in **every measure** of every part
   (e.g. lc6260379: 141 clefs / 564 keysig accidentals / 282 timesig
   components for a 47-measure 3-staff piece whose source has 1 clef line
   and 5 keysig lines). Note categories (pitches, heads, dots, beams,
   accidentals) are byte-identical in 12 of 13 files; the per-file delta
   equals the signature surplus exactly. These are the same pairs whose
   pair-level Tier-A OMR-NED is already ≈0.97 in the oracle (the M1 "max
   0.99" triage seeds) — degenerate inputs, not count disagreements on
   healthy data.
2. **iomusxml drops `<time-modification>` without `<tuplet>` notation**
   (xml undercounts: lc6189652 −164, lc6581327 −424, lc6182149 −119).
   Measured tremolo halves and unnotated ratio changes carry
   time-modification only; music21 creates a duration Tuplet (2 symbols per
   note: type + info) while Verovio's importer discards the ratio entirely
   (iomusxml.cpp builds Tuplet containers only from `<tuplet>` starts).
   Unrepresentable on the C++ side; also implies the affected notes' offsets
   differ — relevant to M3 matching on these files, recorded in
   docs/mei_symbol_mapping.md.
3. **Cross-staff note assignment** (lc6199572 xml +336). music21 and
   Verovio assign cross-staff beamed/triplet notes to different staves of a
   grand-staff part (global pitch counts agree exactly: 1253 = 1253); the
   per-staff flat-measure tuplet machinery then sees different member sets
   and synthesizes different start/continue runs.

Verdict: pred side 0.30% aggregate |Δ| (122/185 exact), gt side 1.53%
(131/185 exact) — both within the ~2% M2 exit criterion, every top-20
outlier explained.
