BUILD_DIR := build
PY := harness/.venv/bin/python
DATA_ROOT := /home/daniel/.local/share/src/Transcoda/main/data/interim
JOBS := 2

.PHONY: build test bench clean corpora oracle-dev200 mutgen mutcheck sweep harness-test count-audit

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

harness-test:
	@test -x $(PY) || { echo "harness venv missing — run harness/setup.sh first"; exit 1; }
	$(PY) -m unittest discover -s harness/tests -v
