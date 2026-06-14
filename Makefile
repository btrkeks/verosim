BUILD_DIR := build
PY := harness/.venv/bin/python
DATA_ROOT := /home/daniel/.local/share/src/Transcoda/main/data/interim
JOBS := 2

.PHONY: build test bench clean corpora oracle-dev200 mutgen mutcheck sweep harness-test count-audit identity-gate corr-audit directions-audit

build:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

# The full test gate (working practice: rungs run on every change) = C++ tests
# + Python harness tests. Use the individual targets to run one side only.
test: build harness-test
	ctest --test-dir $(BUILD_DIR) --output-on-failure

bench:
	@echo "bench: placeholder until M4/M5 (batch mode + perf pass)"

clean:
	rm -rf $(BUILD_DIR)

# ---- M1: corpora + oracle harness (see docs/v1_attack_plan.html section M1) ----

corpora:
	$(PY) -m verosim_harness.sample_corpora --data-root $(DATA_ROOT)

oracle-dev200:
	for d in tierA tierAB tierAB_dir; do \
	  $(PY) -m verosim_harness.run_oracle --corpus corpora/dev200.tsv --detail $$d \
	    --jobs $(JOBS) --data-root $(DATA_ROOT) \
	    --out oracle/dev200_$$d.jsonl --summary-out corpora/summaries/dev200_$$d.json \
	    || exit 1; \
	done

mutgen:
	$(PY) -m verosim_harness.gen_mutations --write

mutcheck:
	$(PY) -m verosim_harness.gen_mutations --verify
	$(PY) -m verosim_harness.mutcheck --out oracle/mutcheck.jsonl

sweep: build
	mkdir -p oracle
	BIN=$(BUILD_DIR)/verosim harness/run_sweep.sh corpora/perf10k.txt $(DATA_ROOT) oracle/sweep_perf10k.jsonl
	BIN=$(BUILD_DIR)/verosim harness/run_sweep.sh corpora/lieder_all_krn.txt $(DATA_ROOT) oracle/sweep_lieder.jsonl
	$(PY) -m verosim_harness.sweep_report oracle/sweep_perf10k.jsonl oracle/sweep_lieder.jsonl \
	  --out docs/parse_coverage_m1.md

# ---- M2: DEV-200 symbol-count audit (Layer 2 counts vs oracle n values) ----

count-audit: build
	$(PY) -m verosim_harness.count_audit --oracle oracle/dev200_tierA.jsonl \
	  --bin $(BUILD_DIR)/verosim --data-root $(DATA_ROOT) \
	  --out oracle/count_audit_dev200.jsonl --report docs/count_audit_m2.md

# ---- M3/M4: comparison-engine gates (a) identity and (c) oracle correlation ----
# (gate (b), the mutation corpus, runs in `make test` via tests/test_mutation_gate.cpp)
# Gate (c) per D15: the engine-fidelity gate is the same-format (engine-
# isolating) Spearman on healthy sources >= 0.95; the xml<->krn DEV-200 audit
# is the measured cross-importer rank agreement with a 0.80 regression floor.

identity-gate: build
	$(PY) -m verosim_harness.identity_audit --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --out oracle/identity_m3.jsonl

corr-audit: build
	$(PY) -m verosim_harness.corr_audit --oracle oracle/dev200_tierAB.jsonl \
	  --bin $(BUILD_DIR)/verosim --data-root $(DATA_ROOT) \
	  --explained corpora/dev200_explained_pairs.tsv --threshold 0.80 \
	  --out oracle/corr_audit_dev200_tierAB.jsonl --report docs/corr_audit_m4.md
	$(PY) -m verosim_harness.synth_corr --bin $(BUILD_DIR)/verosim \
	  --data-root $(DATA_ROOT) --detail tierAB --jobs $(JOBS) --out oracle/synth_corr_m4.jsonl \
	  --explained corpora/dev200_explained_pairs.tsv --threshold 0.95 \
	  --report-append docs/corr_audit_m4.md

# ---- M4: quantify the Directions granularity gap before choosing the gate ----

directions-audit:
	$(PY) -m verosim_harness.directions_audit --oracle oracle/dev200_tierAB_dir.jsonl \
	  --out docs/m4_directions_audit.md

harness-test:
	@test -x $(PY) || { echo "harness venv missing — run harness/setup.sh first"; exit 1; }
	$(PY) -m unittest discover -s harness/tests -v
