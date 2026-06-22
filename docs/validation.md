# Validation Summary

This repository keeps detailed audit outputs reproducible through the harness,
but does not commit them as primary documentation. The current v1 acceptance
evidence is:

| Check | Result |
|---|---|
| Exact fixtures | HAND-10 symbol counts and runnable mutation cases pass exact cost checks. |
| DEV-200 active count audit | Aggregate absolute count delta vs. Python oracle: 0.30% XML side, 1.53% kern side. Top outliers are documented importer differences. |
| DEV-200 active correlation | Cross-importer raw Spearman: 0.8433, above the 0.80 regression floor. Same-format engine-isolating Spearman on healthy sources: 0.9998. |
| HOLDOUT-100 acceptance | Cross-importer raw Spearman: 0.7556, above the 0.75 holdout floor. Same-format engine-isolating Spearman after parser-degenerate exclusions: 0.9995. |
| Parse coverage | Verovio load coverage: openscore-lieder 99.25%, pdmx 99.84%, grandstaff 100.00%. |
| Known clean failures | 25 known PERF/Lieder load failures all emit JSONL records with nonempty error messages. |
| PERF-10K identity benchmark | 1,000 self-pairs, 0 load failures, 0 nonzero identity distances, 113.0 pairs/s with `--jobs 2` on the last local run. |

The correlation gate is intentionally split:

- Same-format comparisons isolate the C++ comparison engine from importer
  differences and are the hard fidelity gate.
- Cross-importer XML↔kern comparisons are reported with regression floors
  because music21/converter21 and Verovio disagree on some malformed or
  ambiguous inputs before the comparison engine sees them.

To regenerate detailed artifacts, run the relevant Make targets. Markdown
reports and JSONL outputs are written under `oracle/`, which is ignored:

```sh
make count-audit
make corr-audit
make holdout-acceptance
make holdout-engine-gate
make robustness-gate
make bench
make acceptance
```
