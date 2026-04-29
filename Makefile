SHELL := /bin/bash

BUILD_DIR ?= build
WEB_DIR ?= web
PAGES_BASE_PATH ?= /chinese-chess/
PREVIEW_HOST ?= 127.0.0.1
PREVIEW_PORT ?= 4173

.PHONY: help configure build test native-cli fake-engine-smoke pikafish engine-smoke \
	wasm web-install web-dev web-build web-preview web-bundle pages-build clean

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make configure          # Configure the native CMake build' \
		'  make build              # Build native targets with CMake' \
		'  make test               # Run the native test suite' \
		'  make native-cli         # Build and run the native CLI smoke target' \
		'  make fake-engine-smoke  # Run the CLI against the fake UCI engine fixture' \
		'  make pikafish           # Build the bundled Pikafish submodule' \
		'  make engine-smoke       # Run the CLI against the real Pikafish binary' \
		'  make wasm               # Build the WebAssembly bridge artifacts' \
		'  make web-install        # Install frontend dependencies' \
		'  make web-dev            # Start the Vite dev server' \
		'  make web-build          # Build the production frontend bundle' \
		'  make web-preview        # Preview the production frontend locally' \
		'  make web-bundle         # Build the frontend via the CMake target' \
		'  make pages-build        # Build the GitHub Pages-targeted frontend bundle' \
		'  make clean              # Remove generated build outputs'

configure:
	cmake -S . -B $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

native-cli: build
	./$(BUILD_DIR)/chinese_chess_cli

fake-engine-smoke: build
	./$(BUILD_DIR)/chinese_chess_cli --engine-cmd "python3 tests/fixtures/fake_uci_engine.py" --depth 1

pikafish:
	./scripts/build_pikafish.sh

engine-smoke: build pikafish
	./$(BUILD_DIR)/chinese_chess_cli --engine-cmd ./third_party/pikafish/src/pikafish --depth 1

wasm:
	./scripts/build_wasm.sh

web-install:
	npm install --prefix $(WEB_DIR)

web-dev:
	npm run dev --prefix $(WEB_DIR)

web-build:
	npm run build --prefix $(WEB_DIR)

web-preview:
	npm run preview --prefix $(WEB_DIR) -- --host $(PREVIEW_HOST) --port $(PREVIEW_PORT)

web-bundle: configure
	cmake --build $(BUILD_DIR) --target web_bundle

pages-build:
	VITE_BASE_PATH=$(PAGES_BASE_PATH) npm run build --prefix $(WEB_DIR)

clean:
	rm -rf $(BUILD_DIR) build-wasm $(WEB_DIR)/dist
