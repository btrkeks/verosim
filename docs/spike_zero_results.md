# Spike Zero results (2026-06-12T11:36:31Z)

- pair: lc28688206 (openscore-lieder, .xml vs paired .krn)
- musicdiff: f413e5e9b749bba8100b79425912b7db4a0f7a72
- python: Python 3.12.9, music21 9.9.2

## Q1: full detail (default allobjects), omrned output — kern via music21 (R2 probe)

- command: `python -m musicdiff /home/daniel/.local/share/src/Transcoda/main/data/interim/train/openscore-lieder/0_raw_xml/lc28688206.xml /home/daniel/.local/share/src/Transcoda/main/data/interim/train/openscore-lieder/1_kern_conversions/lc28688206.krn -o omrned`
- exit code: 0
- wall time: 1.3s

```
{
    "OMR-ED": "232",
    "numSymbolsInPredicted": "1715",
    "numSymbolsInGroundTruth": "1742",
    "numSymbolsInBoth": "3457",
    "OMR-NED": "0.06711021116575065"
}
```

## Q2: Tier A only (-i notesandrests beams signatures)

- command: `python -m musicdiff /home/daniel/.local/share/src/Transcoda/main/data/interim/train/openscore-lieder/0_raw_xml/lc28688206.xml /home/daniel/.local/share/src/Transcoda/main/data/interim/train/openscore-lieder/1_kern_conversions/lc28688206.krn -i notesandrests beams signatures -o omrned`
- exit code: 0
- wall time: 1.1s

```
{
    "OMR-ED": "148",
    "numSymbolsInPredicted": "1095",
    "numSymbolsInGroundTruth": "1097",
    "numSymbolsInBoth": "2192",
    "OMR-NED": "0.06751824817518248"
}
```

## Q3: oracle precompute sizing

- per-pair wall time: full detail 1.3s, Tier A 1.1s
- DEV-200 x 3 detail levels, single-threaded upper bound (full-detail rate): 13 min
- parallel over 16 cores: ~1 min
