BUILD_DIR := build

.PHONY: build test bench clean

build:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

bench:
	@echo "bench: placeholder until M4/M5 (batch mode + perf pass)"

clean:
	rm -rf $(BUILD_DIR)
