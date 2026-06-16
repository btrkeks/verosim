BUILD_DIR := build
STATIC_BUILD_DIR := build-static
PY := harness/.venv/bin/python
DATA_ROOT ?=
JOBS := 2
export PYTHONPATH := harness:$(PYTHONPATH)

.PHONY: build static-build test bench clean clean-static corpora oracle-dev200 oracle-holdout mutgen mutcheck sweep harness-test count-audit identity-gate corr-audit directions-audit holdout-acceptance holdout-engine-gate robustness-gate acceptance require-data-root require-submodules

require-submodules:
	@test -f verovio/cmake/CMakeLists.txt || { echo "Verovio submodule missing; run git submodule update --init --recursive"; exit 1; }
	@test -f musicdiff/setup.py || { echo "musicdiff submodule missing; run git submodule update --init --recursive"; exit 1; }

require-data-root:
	@test -n "$(DATA_ROOT)" || { echo "DATA_ROOT is required for corpus/oracle targets. Example: make acceptance DATA_ROOT=/path/to/data/interim"; exit 1; }
	@test -d "$(DATA_ROOT)" || { echo "DATA_ROOT does not exist: $(DATA_ROOT)"; exit 1; }

build: require-submodules
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

static-build: require-submodules
	cmake -S . -B $(STATIC_BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DVEROSIM_STATIC_VEROVIO=ON
	cmake --build $(STATIC_BUILD_DIR)

# The full test gate (working practice: rungs run on every change) = C++ tests
# + Python harness tests. Use the individual targets to run one side only.
test: build harness-test
	ctest --test-dir $(BUILD_DIR) --output-on-failure

bench: require-data-root build
	$(PY) -m verosim_harness.perf_bench --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --jobs $(JOBS) \
	  --out oracle/reports/perf_bench.md

clean:
	rm -rf $(BUILD_DIR)

clean-static:
	rm -rf $(STATIC_BUILD_DIR)

# ---- Corpus lists and oracle harness ----

corpora: require-data-root
	$(PY) -m verosim_harness.sample_corpora --data-root $(DATA_ROOT)

oracle-dev200: require-data-root
	for d in tierA tierAB tierAB_dir; do \
	  $(PY) -m verosim_harness.run_oracle --corpus corpora/dev200.tsv --detail $$d \
	    --jobs $(JOBS) --data-root $(DATA_ROOT) \
	    --out oracle/dev200_$$d.jsonl --summary-out corpora/summaries/dev200_$$d.json \
	    || exit 1; \
	done

oracle-holdout: require-data-root
	$(PY) -m verosim_harness.run_oracle --corpus corpora/holdout100.tsv --detail tierAB \
	  --jobs $(JOBS) --data-root $(DATA_ROOT) \
	  --out oracle/holdout100_tierAB.jsonl --summary-out corpora/summaries/holdout100_tierAB.json

mutgen:
	$(PY) -m verosim_harness.gen_mutations --write

mutcheck:
	$(PY) -m verosim_harness.gen_mutations --verify
	$(PY) -m verosim_harness.mutcheck --out oracle/mutcheck.jsonl

sweep: require-data-root build
	mkdir -p oracle
	BIN=$(BUILD_DIR)/verosim harness/run_sweep.sh corpora/perf10k.txt $(DATA_ROOT) oracle/sweep_perf10k.jsonl
	BIN=$(BUILD_DIR)/verosim harness/run_sweep.sh corpora/lieder_all_krn.txt $(DATA_ROOT) oracle/sweep_lieder.jsonl
	$(PY) -m verosim_harness.sweep_report oracle/sweep_perf10k.jsonl oracle/sweep_lieder.jsonl \
	  --out oracle/reports/parse_coverage.md

# ---- DEV-200 Tier-A symbol-count audit ----

count-audit: require-data-root build
	$(PY) -m verosim_harness.count_audit --oracle oracle/dev200_tierA.jsonl \
	  --bin $(BUILD_DIR)/verosim --data-root $(DATA_ROOT) \
	  --out oracle/count_audit_dev200.jsonl --report oracle/reports/tier_a_count_audit.md

# ---- Comparison-engine validation gates ----
# (gate (b), the mutation corpus, runs in `make test` via tests/test_mutation_gate.cpp)
# Gate (c) per D15: the engine-fidelity gate is the same-format (engine-
# isolating) Spearman on healthy sources >= 0.95; the xml<->krn DEV-200 audit
# is the measured cross-importer rank agreement with a 0.80 regression floor.

identity-gate: require-data-root build
	$(PY) -m verosim_harness.identity_audit --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --out oracle/identity_gate.jsonl

corr-audit: require-data-root build
	$(PY) -m verosim_harness.corr_audit --oracle oracle/dev200_tierAB.jsonl \
	  --bin $(BUILD_DIR)/verosim --data-root $(DATA_ROOT) \
	  --explained corpora/dev200_explained_pairs.tsv --threshold 0.80 \
	  --out oracle/corr_audit_dev200_tierAB.jsonl --report oracle/reports/tier_ab_correlation_audit.md
	$(PY) -m verosim_harness.synth_corr --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --detail tierAB --jobs $(JOBS) --out oracle/synth_corr_tierAB.jsonl \
	  --explained corpora/dev200_explained_pairs.tsv --threshold 0.95 \
	  --report-append oracle/reports/tier_ab_correlation_audit.md

# ---- Directions detail-level diagnostic ----

directions-audit:
	$(PY) -m verosim_harness.directions_audit --oracle oracle/dev200_tierAB_dir.jsonl \
	  --out oracle/reports/directions_detail_audit.md

# ---- Final acceptance, robustness, and performance ----

holdout-acceptance: require-data-root build oracle-holdout
	$(PY) -m verosim_harness.corr_audit --oracle oracle/holdout100_tierAB.jsonl \
	  --bin $(BUILD_DIR)/verosim --data-root $(DATA_ROOT) --detail tierAB \
	  --threshold 0.75 --out oracle/holdout_acceptance_tierAB.jsonl \
	  --report oracle/reports/holdout_acceptance.md

holdout-engine-gate: require-data-root build
	$(PY) -m verosim_harness.synth_corr --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --corpus corpora/holdout100.tsv --detail tierAB \
	  --jobs $(JOBS) --n 100 --out oracle/holdout_synth_corr_tierAB.jsonl \
	  --exclude-oracle-degenerate-threshold 0.9 --threshold 0.95 \
	  --report-append oracle/reports/holdout_acceptance.md --workdir oracle/holdout_synth_work

robustness-gate: require-data-root build
	$(PY) -m verosim_harness.robustness_audit --bin $(BUILD_DIR)/verosim \
	  --files-from corpora/known_parse_failures.txt --data-root $(DATA_ROOT) \
	  --out oracle/reports/robustness_audit.md

acceptance: test holdout-acceptance holdout-engine-gate robustness-gate bench

harness-test:
	@test -x $(PY) || { echo "harness venv missing — run harness/setup.sh first"; exit 1; }
	PYTHONPATH=harness $(PY) -m unittest discover -s harness/tests -v
