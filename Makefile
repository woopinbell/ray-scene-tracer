BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: all clean test

all:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD_DIR)
	$(CMAKE) -E create_symlink $(BUILD_DIR)/ray-scene-tracer ray-scene-tracer

test:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR) ray-scene-tracer
