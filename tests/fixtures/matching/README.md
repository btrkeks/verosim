# Matching regression inputs

`mozart-k331-p13.*.krn` freezes the prepared prediction from
`v22c-real-s7294-w73-dev` and current reference for
`imslp__mozart-piano-sonata-no11-in-a-major-k-331-300i-mozart-wolfgang-amadeus-imslp220__p0013`,
investigated on 2026-11-06. Reference corpus revision:
`sha256:430d7066266d0be5933a259885f7cb661d9f33f674bf06b4861c8ab2a52e9756`.

The prediction is missing the boundary after the A–B octave pickup in the
fifth system. Reference bars 26 and 27 (zero-based) correspond to predicted
bar 26 on both staffs. The next right-hand quarter-note octave is also
wrong: A instead of C-sharp. Other prediction errors remain elsewhere.
The regression must preserve those note errors while identifying the
boundary error without deleting an entire measure.
