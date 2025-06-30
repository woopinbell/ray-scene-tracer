BUILD_DIR ?= build
SANITIZER_BUILD_DIR ?= build/sanitize
CMAKE ?= cmake
CTEST ?= ctest
CXX := c++
BUILD_TYPE ?= Release
RAY_ENABLE_SANITIZERS ?= OFF
CMAKE_CONFIGURE_ARGS ?=
CMAKE_BUILD_ARGS ?= --parallel
CTEST_ARGS ?= --output-on-failure --no-tests=error --timeout 120

.PHONY: all build check ci clean configure guard-build-dir help sanitize test

all: build
	$(CMAKE) -E rm -f ray-scene-tracer
	$(CMAKE) -E create_symlink "$(BUILD_DIR)/ray-scene-tracer" ray-scene-tracer

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" \
		"-DCMAKE_CXX_COMPILER=$(CXX)" \
		"-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)" \
		-DBUILD_TESTING=ON \
		"-DRAY_ENABLE_SANITIZERS=$(RAY_ENABLE_SANITIZERS)" \
		$(CMAKE_CONFIGURE_ARGS)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" $(CMAKE_BUILD_ARGS)

test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" $(CTEST_ARGS)

check: test

ci:
	$(MAKE) clean
	$(MAKE) check

sanitize:
	$(MAKE) BUILD_DIR="$(SANITIZER_BUILD_DIR)" BUILD_TYPE=Debug \
		RAY_ENABLE_SANITIZERS=ON ci

help:
	@printf '%s\n' \
		'Targets:' \
		'  all       Configure and build a release binary.' \
		'  test      Build and run the complete CTest suite.' \
		'  check     Run the complete functional test gate.' \
		'  ci        Clean, rebuild, and run the functional gate.' \
		'  sanitize  Run the gate with AddressSanitizer and UBSan.' \
		'  clean     Remove generated outputs from BUILD_DIR.' \
		'' \
		'Overrides: CXX, CMAKE, CTEST, BUILD_DIR, BUILD_TYPE,' \
		'           CMAKE_CONFIGURE_ARGS, CMAKE_BUILD_ARGS, CTEST_ARGS'

guard-build-dir:
	@build_dir="$(abspath $(BUILD_DIR))"; \
	case "$$build_dir" in \
		"$(CURDIR)"/*) ;; \
		*) printf 'Refusing to remove unsafe BUILD_DIR: %s\n' \
			"$$build_dir" >&2; exit 2 ;; \
	esac

clean: guard-build-dir
	$(CMAKE) -E rm -rf "$(BUILD_DIR)"
	$(CMAKE) -E rm -f ray-scene-tracer
