# Verovio parse-coverage sweep (M1)

Generated 2026-06-12 by `verosim --check --files-from <list>` + `verosim_harness.sweep_report` over: sweep_perf10k.jsonl, sweep_lieder.jsonl.

Counts are per file; warning counts are unique-messages-per-file (Verovio dedups identical log lines within one load).

| dataset | files | loaded ok | % ok | with warnings | % warn | exceptions |
|---|---|---|---|---|---|---|
| train/grandstaff | 1507 | 1507 | 100.00% | 0 | 0.00% | 0 |
| train/openscore-lieder | 1462 | 1451 | 99.25% | 111 | 7.59% | 11 |
| train/pdmx | 8493 | 8479 | 99.84% | 96 | 1.13% | 14 |

## train/grandstaff

No warnings.

## train/openscore-lieder

### load failures (11)

- `train/openscore-lieder/1_kern_conversions/lc5025982.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc5098709.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc5712146.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6197282.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6218728.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6236154.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6474919.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6569855.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6573515.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6575466.krn` — exception: vector::_M_default_append
- `train/openscore-lieder/1_kern_conversions/lc6974646.krn` — exception: vector::_M_default_append

### top warnings (380 total, 316 distinct)

- 9× In HumdrumInput::setDuration: Unprintable duration2/3 quarter notes
- 9× In HumdrumInput::setDuration: Unprintable duration1/3 quarter notes
- 8× In HumdrumInput::setDuration: Unprintable duration1/6 quarter notes
- 6×    Element stack: 
- 6×       0:	layer
- 5× In HumdrumInput::removeBeam: Error removing beam
- 5×       2:	tuplet
- 5×       1:	beam
- 5× In HumdrumInput::setDuration: Unprintable duration5/6 quarter notes
- 4× In HumdrumInput::setDuration: Unprintable duration5/8 quarter notes
- 4× In HumdrumInput::fillContentsOfLayer: Strange error when adding rest .
- 4× In HumdrumInput::setDuration: Unprintable duration7/8 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration11/12 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration1/12 quarter notes
- 2× In HumdrumInput::setDuration: Unprintable duration3/8 quarter notes
- 2× In HumdrumInput::setDuration: Unprintable duration7/12 quarter notes
- 1× octave 'ta98zsv' is not encoded in the measure of its start 'chord-L61F2'. This may cause improper rendering.
- 1× octave 'qjqp6hi' is not encoded in the measure of its start 'chord-L22F2'. This may cause improper rendering.
- 1× octave 'so5m4qr' is not encoded in the measure of its start 'chord-L38F2'. This may cause improper rendering.
- 1× octave 'n1odegeu' is not encoded in the measure of its start 'chord-L55F2'. This may cause improper rendering.

## train/pdmx

### load failures (14)

- `train/pdmx/1_kern_conversions/QmYL1oFYbvNjSy1YSP7GbxBYTx2As6Gv7nUeJSMqfjU2d9.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmSYzHmvKkcy7LkVnmH9BejKfcJzVkZgNTZCt38cWauoVW.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/Qmdqsq9dVoStuQfHfvayroMqSAbzSykSqfThobSmq7qtsg.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmVoKrgBQcPE7esc31XkGSxWasEiU65HuW2wJwRMqgXCQs.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmfKwojamTzc9eL9SsC4vdYxD1vyVHJuJBbJfMkPPNDvR5.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmThGK4no2yoKL6nvFa2o8HFabMcFLphJVvcoBazsPfs6W.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmXHGMWs1b1zu9pRZ5ZAU2HWTEmrADb3ijAopS1Q4K1ef7.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmURMz6Gn4sAamnR3b7zpjgri2XQg65NKQ9LdYKZGXi1FC.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmUcwYeV2QahFGXQZhyrevagLGgRFgQtpvXJ3r475RkjYj.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmXGpmdbEE6hAHHNdQ9JuvvHAhwNyXZHfgWEDoPM5gRPUN.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmTtsF2HGFVd6onPuiUWhBVrHhd8CpvCnezm29MS6L4gEE.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmWmiqUKZZpS2zF8jKRna8QfnFv2k4eiwwLrzze6wofST6.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmaHDXERTd4C2j6RvvqM5dXUysr2rzxeDhmbaAE4D1Kcdb.krn` — exception: vector::_M_default_append
- `train/pdmx/1_kern_conversions/QmasDRy7rFCGZmvQtNxAW2AgRT8iCveUw1JDjW3gBn9gNm.krn` — exception: vector::_M_default_append

### top warnings (675 total, 522 distinct)

- 20×    Element stack: 
- 20×       0:	layer
- 18× In HumdrumInput::setDuration: Unprintable duration4/55 quarter notes
- 12× In HumdrumInput::removeBeam: Error removing beam
- 12×       2:	tuplet
- 11×       1:	beam
- 11× In HumdrumInput::removeTuplet: Error removing tuplet
- 10×       2:	beam
- 9×    Last element is: beam
- 9×       1:	tuplet
- 5× In HumdrumInput::setDuration: Unprintable duration3/8 quarter notes
- 5× In HumdrumInput::fillContentsOfLayer: Strange error when adding rest .
- 4× In HumdrumInput::setDuration: Unprintable duration5/8 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration2/3 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration9/16 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration11/16 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration7/16 quarter notes
- 3× In HumdrumInput::setDuration: Unprintable duration1/3 quarter notes
- 2× In HumdrumInput::setDuration: Unprintable duration35/96 quarter notes
- 2× In HumdrumInput::setDuration: Unprintable duration31/32 quarter notes
