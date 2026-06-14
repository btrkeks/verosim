# M4 Directions Detail-Level Audit

Generated from `oracle/dev200_tierAB_dir.jsonl` by `verosim_harness.directions_audit`.

- records: 200
- usable oracle records: 186
- oracle errors: 14

## Interpretation

The `tierAB_dir` oracle run is useful diagnostic evidence, but it is not a clean Tier-B gate: musicdiff's Directions bit includes v1 dynamics/hairpins as well as non-v1 text directions, pedal marks, endings, tempos, and rehearsal marks. Per D16, M4 gates correlation on the cleaner `tierAB` level and reports this decomposition separately.

## Extra Cost By Kind

| kind | cost |
|---|---:|
| `direction` | 9612 |
| `pedalmark` | 2584 |
| `slur` | 1108 |
| `dynamic` | 1045 |
| `clef` | 909 |
| `diminuendo` | 648 |
| `crescendo` | 490 |
| `keysig` | 421 |
| `timesig` | 248 |
| `ending` | 140 |
| `tempo` | 29 |
| `rehearsalmark` | 14 |
| `pedalgapstart` | 6 |

## Header Totals

| header | cost |
|---|---:|
| entire measure insert/delete OMR-ED | 91712 |
| wrong note OMR-ED | 38843 |
| wrong direction OMR-ED | 12216 |
| wrong tuplet OMR-ED | 2014 |
| wrong flag/beam OMR-ED | 1655 |
| wrong graceness OMR-ED | 1356 |
| wrong slur OMR-ED | 1108 |
| wrong dynamic OMR-ED | 1045 |
| wrong clef OMR-ED | 909 |
| wrong diminuendo OMR-ED | 648 |
| wrong crescendo OMR-ED | 490 |
| wrong keysig OMR-ED | 421 |
| wrong note head OMR-ED | 250 |
| wrong timesig OMR-ED | 248 |
| wrong accidental OMR-ED | 156 |
| wrong ending OMR-ED | 140 |
| wrong dot OMR-ED | 84 |
| wrong tempo OMR-ED | 29 |
| wrong articulation OMR-ED | 22 |
| wrong tie OMR-ED | 15 |

## Extra Cost By Kind And Op

| kind | op | cost |
|---|---|---:|
| `direction` | `extrains` | 4571 |
| `direction` | `extradel` | 4365 |
| `pedalmark` | `extradel` | 2584 |
| `dynamic` | `extradel` | 709 |
| `direction` | `extracontentedit` | 676 |
| `slur` | `extradel` | 574 |
| `clef` | `extrains` | 519 |
| `slur` | `extrains` | 499 |
| `keysig` | `extrains` | 398 |
| `clef` | `extradel` | 384 |
| `dynamic` | `extrains` | 254 |
| `timesig` | `extrains` | 245 |
| `diminuendo` | `extradel` | 239 |
| `crescendo` | `extradurationedit` | 237 |
| `diminuendo` | `extradurationedit` | 223 |
| `diminuendo` | `extrains` | 186 |
| `crescendo` | `extradel` | 155 |
| `ending` | `extradel` | 140 |
| `crescendo` | `extrains` | 98 |
| `dynamic` | `extrasymboledit` | 82 |
| `slur` | `extradurationedit` | 35 |
| `tempo` | `extradel` | 27 |
| `keysig` | `extrainfoedit` | 23 |
| `rehearsalmark` | `extradel` | 14 |
| `clef` | `extrasymboledit` | 6 |
| `pedalgapstart` | `extradel` | 6 |
| `timesig` | `extrainfoedit` | 3 |
| `tempo` | `extrasymboledit` | 2 |

## Examples

### `clef`

- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 1 `Extra(140716569905920):clef,symbol=F4,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 1 `Extra(140716569655488):clef,symbol=F4,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extrains` cost 1 `Extra(140716566528944):clef,symbol=F4,off=4.0`

### `crescendo`

- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extrains` cost 1 `Extra(139818081030976):crescendo,off=0.0,dur=1.0`
- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extrains` cost 1 `Extra(139818081801120):crescendo,off=2.0,dur=1.5`
- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extrains` cost 1 `Extra(139818080534896):crescendo,off=0.0,dur=1.5`

### `diminuendo`

- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradurationedit` cost 1 `Extra(140717304402992):diminuendo,off=1.0,dur=1.5`<br>`Extra(140716565470800):diminuendo,off=1.0,dur=1.25`
- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extrains` cost 1 `Extra(139818081031312):diminuendo,off=1.0,dur=0.75`
- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extrains` cost 1 `Extra(139818080536192):diminuendo,off=1.5,dur=2.0`

### `direction`

- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extrains` cost 5 `Extra(140716563967696):direction,content=rall.,off=2.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extrains` cost 23 `Extra(140716561845712):direction,content=Enchainez avec le No. 2,off=1.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 5 `Extra(140716569916720):direction,content=rall.,off=2.0`

### `dynamic`

- `train/openscore-lieder/0_raw_xml/lc5013945.xml`: `extrains` cost 1 `Extra(139818106332672):dynamic,symbol=pp,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc5013945.xml`: `extradel` cost 1 `Extra(139818104212464):dynamic,symbol=pp,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 1 `Extra(140716569309600):dynamic,symbol=other-dynamics,off=0.0`

### `ending`

- `train/openscore-lieder/0_raw_xml/lc6997609.xml`: `extradel` cost 4 `Extra(139821487037408):ending,content=1.,off=0.0,dur=20.0,info:,measurecount=2`
- `train/openscore-lieder/0_raw_xml/lc6997609.xml`: `extradel` cost 4 `Extra(139821487039136):ending,content=2.,off=0.0,dur=16.0,info:,measurecount=2`
- `train/openscore-lieder/0_raw_xml/lc6174661.xml`: `extradel` cost 4 `Extra(139820568857536):ending,content=3.,off=0.0,dur=4.0,info:,measurecount=1`

### `keysig`

- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 5 `Extra(140716561033664):keysig,off=0.0,info:,flat0=B,flat1=E,flat2=A,flat3=D,flat4=G`
- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 5 `Extra(140716570711712):keysig,off=0.0,info:,flat0=B,flat1=E,flat2=A,flat3=D,flat4=G`
- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 5 `Extra(140716570711712):keysig,off=0.0,info:,flat0=B,flat1=E,flat2=A,flat3=D,flat4=G`

### `pedalgapstart`

- `train/openscore-lieder/0_raw_xml/lc6067231.xml`: `extradel` cost 1 `Extra(139817813577072):pedalgapstart,symbol=PedalGapStart,off=3.0`
- `train/openscore-lieder/0_raw_xml/lc6067231.xml`: `extradel` cost 1 `Extra(139817813581056):pedalgapstart,symbol=PedalGapStart,off=3.0`
- `train/openscore-lieder/0_raw_xml/lc6581327.xml`: `extradel` cost 1 `Extra(139815213391632):pedalgapstart,symbol=PedalGapStart,off=0.5`

### `pedalmark`

- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 4 `Extra(140716567210336):pedalmark,off=0.0,dur=4.0,info:,pedalType=sustain,pedalForm=symbol`
- `train/openscore-lieder/0_raw_xml/lc6726319.xml`: `extradel` cost 4 `Extra(139818074401600):pedalmark,off=1.0,dur=1.0,info:,pedalType=sustain,pedalForm=symbol`
- `train/openscore-lieder/0_raw_xml/lc6800506.xml`: `extradel` cost 4 `Extra(140712484225344):pedalmark,off=0.0,dur=3.0,info:,pedalType=sustain,pedalForm=symbol`

### `rehearsalmark`

- `train/openscore-lieder/0_raw_xml/lc6211559.xml`: `extradel` cost 2 `Extra(139815039030992):rehearsalmark,content=1,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc6211559.xml`: `extradel` cost 2 `Extra(139817941190080):rehearsalmark,content=2,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc6211559.xml`: `extradel` cost 2 `Extra(139817941199488):rehearsalmark,content=3,off=0.0`

### `slur`

- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extrains` cost 1 `Extra(140716564383312):slur,off=0.0,dur=2.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 1 `Extra(140716567212640):slur,off=0.0,dur=4.0`
- `train/openscore-lieder/0_raw_xml/lc5852737.xml`: `extradel` cost 1 `Extra(140716567213408):slur,off=3.5,dur=0.5`

### `tempo`

- `train/openscore-lieder/0_raw_xml/lc6688176.xml`: `extradel` cost 1 `Extra(139818097341216):tempo,symbol=Quarter=84.0,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc6726319.xml`: `extrasymboledit` cost 2 `Extra(139818097785168):tempo,symbol=Eighth=80.0,off=0.0`<br>`Extra(139818072945152):tempo,symbol=8th=80.0,off=0.0`
- `train/openscore-lieder/0_raw_xml/lc6888209.xml`: `extradel` cost 1 `Extra(140716562946176):tempo,symbol=Quarter=63.0,off=0.0`

### `timesig`

- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 1 `Extra(140716561035536):timesig,off=0.0,info:,symbol=common`
- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 1 `Extra(140716570278080):timesig,off=0.0,info:,symbol=common`
- `train/openscore-lieder/0_raw_xml/lc6510502.xml`: `extrains` cost 1 `Extra(140716570278080):timesig,off=0.0,info:,symbol=common`
