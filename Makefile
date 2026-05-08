SHELL := /bin/bash

BUILD_TYPE ?= Debug
BUILD_TYPE_LOWER := $(shell printf '%s' "$(BUILD_TYPE)" | tr '[:upper:]' '[:lower:]')
BUILD_DIR ?= build/$(BUILD_TYPE_LOWER)
WASM_BUILD_DIR ?= build-wasm/$(BUILD_TYPE_LOWER)
WEB_DIR ?= web
PAGES_BASE_PATH ?= /chinese-chess/
PREVIEW_HOST ?= 127.0.0.1
PREVIEW_PORT ?= 4173

.PHONY: help \
	configure configure-debug configure-release \
	build build-debug build-release \
	test test-debug test-release \
	native-cli native-cli-debug native-cli-release \
	fake-engine-smoke fake-engine-smoke-debug fake-engine-smoke-release \
	pikafish \
	engine-smoke engine-smoke-debug engine-smoke-release \
	wasm wasm-debug wasm-release \
	web-install web-dev web-build web-preview web-bundle pages-build clean

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make configure          # Configure the Debug native CMake build' \
		'  make configure-debug    # Configure the Debug native CMake build' \
		'  make configure-release  # Configure the Release native CMake build' \
		'  make build              # Build Debug native targets with CMake' \
		'  make build-debug        # Build Debug native targets with CMake' \
		'  make build-release      # Build Release native targets with CMake' \
		'  make test               # Run the Debug native test suite' \
		'  make test-debug         # Run the Debug native test suite' \
		'  make test-release       # Run the Release native test suite' \
		'  make native-cli         # Build and run the Debug CLI smoke target' \
		'  make native-cli-release # Build and run the Release CLI smoke target' \
		'  make fake-engine-smoke  # Run the Debug CLI against the fake UCI engine fixture' \
		'  make fake-engine-smoke-release # Run the Release CLI against the fake UCI engine fixture' \
		'  make pikafish           # Build the vendored Pikafish engine source' \
		'  make engine-smoke       # Run the Debug CLI against the real Pikafish binary' \
		'  make engine-smoke-release # Run the Release CLI against the real Pikafish binary' \
		'  make wasm               # Build the Debug WebAssembly bridge artifacts' \
		'  make wasm-debug         # Build the Debug WebAssembly bridge artifacts' \
		'  make wasm-release       # Build the Release WebAssembly bridge artifacts' \
		'  make web-install        # Install frontend dependencies' \
		'  make web-dev            # Start the Vite dev server' \
		'  make web-build          # Build the production frontend bundle' \
		'  make web-preview        # Preview the production frontend locally' \
		'  make web-bundle         # Build the frontend via the CMake target' \
		'  make pages-build        # Build the Release WASM bridge and the GitHub Pages frontend bundle' \
		'  make clean              # Remove generated build outputs'

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

configure-debug:
	$(MAKE) configure BUILD_TYPE=Debug

configure-release:
	$(MAKE) configure BUILD_TYPE=Release

build: configure
	cmake --build $(BUILD_DIR) --config $(BUILD_TYPE)

build-debug:
	$(MAKE) build BUILD_TYPE=Debug

build-release:
	$(MAKE) build BUILD_TYPE=Release

test: build
	ctest --test-dir $(BUILD_DIR) -C $(BUILD_TYPE) --output-on-failure

test-debug:
	$(MAKE) test BUILD_TYPE=Debug

test-release:
	$(MAKE) test BUILD_TYPE=Release

native-cli: build
	./$(BUILD_DIR)/chinese_chess_cli

native-cli-debug:
	$(MAKE) native-cli BUILD_TYPE=Debug

native-cli-release:
	$(MAKE) native-cli BUILD_TYPE=Release

fake-engine-smoke: build
	./$(BUILD_DIR)/chinese_chess_cli --engine-cmd "python3 tests/fixtures/fake_uci_engine.py" --depth 1

fake-engine-smoke-debug:
	$(MAKE) fake-engine-smoke BUILD_TYPE=Debug

fake-engine-smoke-release:
	$(MAKE) fake-engine-smoke BUILD_TYPE=Release

pikafish:
	./scripts/build_pikafish.sh

engine-smoke: build pikafish
	./$(BUILD_DIR)/chinese_chess_cli --engine-cmd ./third_party/pikafish/src/pikafish --depth 1

engine-smoke-debug:
	$(MAKE) engine-smoke BUILD_TYPE=Debug

engine-smoke-release:
	$(MAKE) engine-smoke BUILD_TYPE=Release

wasm:
	BUILD_TYPE=$(BUILD_TYPE) WASM_BUILD_DIR=$(WASM_BUILD_DIR) ./scripts/build_wasm.sh

wasm-debug:
	$(MAKE) wasm BUILD_TYPE=Debug

wasm-release:
	$(MAKE) wasm BUILD_TYPE=Release

web-install:
	npm install --prefix $(WEB_DIR)

web-dev:
	npm run dev --prefix $(WEB_DIR)

web-build:
	npm run build --prefix $(WEB_DIR)

web-preview:
	npm run preview --prefix $(WEB_DIR) -- --host $(PREVIEW_HOST) --port $(PREVIEW_PORT)

web-bundle: configure
	cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) --target web_bundle

pages-build: wasm-release
	VITE_BASE_PATH=$(PAGES_BASE_PATH) npm run build --prefix $(WEB_DIR)

clean:
	rm -rf $(BUILD_DIR) build-wasm $(WEB_DIR)/dist
