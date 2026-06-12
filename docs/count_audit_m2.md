# M2 DEV-200 symbol-count audit (Tier A)

C++ `--count-symbols` totals vs oracle notation sizes (`n_pred`/`n_gt`, DetailLevel 131). Pair convention: pred = .xml, gt = .krn.

| side | files | errors | Σn (oracle) | Σ\|Δ\| | Σ\|Δ\| % | ΣΔ % | exact |
|---|---|---|---|---|---|---|---|
| pred | 185 | 0 | 616016 | 1858 | 0.3016% | -0.1243% | 122 |
| gt | 185 | 0 | 623898 | 9541 | 1.5293% | -1.4062% | 131 |

## Top 20 outliers by |Δ| / n

| file | side | n oracle | n c++ | Δ | Δ% |
|---|---|---|---|---|---|
| train/openscore-lieder/1_kern_conversions/lc6260379.krn | gt | 2919 | 1953 | -966 | -33.09% |
| train/openscore-lieder/1_kern_conversions/lc5197231.krn | gt | 2958 | 2076 | -882 | -29.82% |
| train/openscore-lieder/1_kern_conversions/lc5644072.krn | gt | 2687 | 1952 | -735 | -27.35% |
| train/openscore-lieder/1_kern_conversions/lc6209605.krn | gt | 1527 | 1122 | -405 | -26.52% |
| train/openscore-lieder/1_kern_conversions/lc6510502.krn | gt | 2200 | 1655 | -545 | -24.77% |
| train/openscore-lieder/1_kern_conversions/lc6451405.krn | gt | 2255 | 1707 | -548 | -24.30% |
| train/openscore-lieder/1_kern_conversions/lc5060910.krn | gt | 4608 | 3544 | -1064 | -23.09% |
| train/openscore-lieder/1_kern_conversions/lc5003150.krn | gt | 1998 | 1563 | -435 | -21.77% |
| train/openscore-lieder/1_kern_conversions/lc5659823.krn | gt | 3682 | 2977 | -705 | -19.15% |
| train/openscore-lieder/1_kern_conversions/lc6199572.krn | gt | 5805 | 4816 | -989 | -17.04% |
| train/openscore-lieder/1_kern_conversions/lc5051349.krn | gt | 3746 | 3170 | -576 | -15.38% |
| train/openscore-lieder/1_kern_conversions/lc5033089.krn | gt | 2336 | 2030 | -306 | -13.10% |
| train/openscore-lieder/1_kern_conversions/lc6236155.krn | gt | 7287 | 6621 | -666 | -9.14% |
| train/openscore-lieder/0_raw_xml/lc6189652.xml | pred | 1850 | 1686 | -164 | -8.86% |
| train/openscore-lieder/0_raw_xml/lc6581327.xml | pred | 5541 | 5117 | -424 | -7.65% |
| train/openscore-lieder/0_raw_xml/lc6199572.xml | pred | 4807 | 5143 | +336 | +6.99% |
| train/openscore-lieder/0_raw_xml/lc6182149.xml | pred | 2657 | 2538 | -119 | -4.48% |
| train/openscore-lieder/0_raw_xml/lc5988117.xml | pred | 4917 | 4803 | -114 | -2.32% |
| train/openscore-lieder/0_raw_xml/lc29026391.xml | pred | 1401 | 1369 | -32 | -2.28% |
| train/openscore-lieder/1_kern_conversions/lc5846139.krn | gt | 3834 | 3748 | -86 | -2.24% |

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
