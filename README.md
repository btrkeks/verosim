# VeroSim

VeroSim is a C++20 implementation of the OMR-NED score-difference metric. It
uses Verovio as the parser for MusicXML, MEI, Humdrum/kern, and related score
formats, extracts an OMR-NED symbol model, and compares scores with a
musicdiff-compatible edit-distance engine.

## Build and Test

Prerequisites: CMake, Ninja, a C++20 compiler, Python 3.12, and `uv` for the
Python oracle harness.

For a first-time checkout:

```sh
git clone --recurse-submodules https://github.com/btrkeks/verosim.git
cd verosim
```

If you already cloned without submodules, initialize them before building:

```sh
git submodule update --init --recursive
```

Set up the Python harness once, then build and test:

```sh
harness/setup.sh
make build
make test
```

The build uses CMake + Ninja and writes binaries under `build/`. The test target
runs the C++ Catch2 suite plus the Python harness tests.

## CLI Usage

Compare two score files. File inputs are loaded through Verovio autodetection,
so the same commands work with MusicXML (`.xml`, `.musicxml`, `.mxl`), MEI
(`.mei`), Humdrum/kern (`.krn`), and other formats supported by the bundled
Verovio parser:

```sh
build/verosim pred.xml gt.krn
build/verosim pred.mxl gt.mei --ops --detail tierAB
build/verosim pred.krn gt.krn --typed-space-handling preserve
```

Write a visual OMR-NED report for a single pair:

```sh
build/verosim --visualize pred.xml gt.krn --out report.html
build/verosim --visualize pred.xml gt.krn --out-dir visual-bundle --output-format svg
```

The report compares the pair with the same OMR-NED engine, renders the
prediction and ground truth through Verovio, and writes a side-by-side HTML file
with annotated SVG pages. Inserted, deleted, and changed notation elements are
highlighted in the score, with the overall OMR-NED, raw edit distance, symbol
counts, edit-distance categories, and any parser or unresolved-mark warnings at
the top of the page. `--detail tierA|tierAB|tierAB_dir` can be added to match
the comparison tier used for the report. The SVG bundle form writes raw
annotated pages under `prediction/` and `ground_truth/`, plus a
`visualization.json` manifest with relative page paths.

Batch compare a TSV of `pred<TAB>gt` rows:

```sh
build/verosim --batch corpora/dev200.tsv --base-dir /path/to/data --jobs 4
```

Check parser robustness over one file or a file list:

```sh
build/verosim --check score.musicxml
build/verosim --check --files-from corpora/perf10k.txt --base-dir /path/to/data
```

Count extracted Tier-A symbols:

```sh
build/verosim --count-symbols score.mei
build/verosim --count-symbols --per-measure score.krn
```

Detail tiers are `tierA`, `tierAB`, and `tierAB_dir`. The v1 acceptance gate uses
`tierAB`; `tierAB_dir` is a diagnostic run for the broader musicdiff Directions
bit.

Typed Verovio spaces can be controlled with
`--typed-space-handling preserve|suppress-straddle-filler`. The default is
`suppress-straddle-filler`, which keeps the legacy workaround for Verovio
`type=straddle`/`type=filler` rhythm-repair spaces. Use `preserve` to keep those
spaces as normal hidden-duration carriers.

The exception is `--batch-jsonl`, which compares in-memory score strings from
JSONL records and currently expects Humdrum/kern text (`--format kern`).

## Documentation

- [Validation summary](docs/validation.md)
- [MEI to OMR-NED symbol mapping](docs/symbol_mapping.md)

## Harness Targets

Set `DATA_ROOT` for targets that use the Transcoda interim corpus:

```sh
export DATA_ROOT=/path/to/data/interim
make oracle-dev200
make corr-audit
make oracle-holdout
make holdout-acceptance
make holdout-engine-gate
make robustness-gate
make bench
make acceptance
```

`make acceptance` runs the exact test gate, HOLDOUT-100 cross-importer
audit, HOLDOUT-100 same-format engine gate, known clean-failure regression
audit, and PERF-10K identity benchmark. Detailed Markdown reports and JSONL
artifacts are written under `oracle/`, which is ignored by git.
