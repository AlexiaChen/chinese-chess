# Chinese Chess — Project Knowledge Base

## Current State

- Repository now has a native CMake skeleton, a portable rules core, a CLI target, and a test target.
- The active goal is to build a Chinese chess game with human-vs-AI play, local execution, and a web build deployable to GitHub Pages.

## Architecture

- Shared C++ core built with CMake.
- Current implemented modules:
  - `src/core/game.h/.cpp`: board state, FEN parsing, legal move generation, move application, and check detection
  - `src/engine/uci_codec.h/.cpp`: bridge between internal coordinates and Pikafish-style UCI square/move strings
  - `src/engine/pikafish_process.h/.cpp`: native UCI subprocess adapter for a Pikafish-compatible engine command
- `src/bridge/browser_session.h/.cpp`: browser-facing session wrapper over the core rules engine
- `src/bridge/wasm_exports.cpp`: C ABI surface exported to the browser/WASM runtime (`current_fen`, `legal_moves_from`, `apply_move`, `reset`)
  - `src/apps/cli/main.cpp`: native CLI target that can print the board or query a configured engine command for `bestmove`
  - `tests/game_tests.cpp`: rules and codec coverage
  - `tests/fixtures/fake_uci_engine.py`: fake UCI engine used for adapter tests
  - `third_party/pikafish`: official Pikafish source as a git submodule
- `web/`: Vue 3 + Vite + TypeScript + Tailwind CSS + Phaser frontend, with Phaser as the render layer and DOM controls for interaction/testability
  - `.github/workflows/ci.yml`: GitHub Actions CI for native tests plus WASM/frontend builds
  - `.github/workflows/pages.yml`: GitHub Pages build and deploy workflow
- Target architecture remains:
  - native targets for engine/testing on WSL2
  - a WebAssembly target for browser play
  - a thin browser UI where Phaser renders the board and DOM controls drive moves

## Build & Run

- Build system target: CMake.
- Default command entrypoint: top-level `Makefile`
- Configure: `make configure`
- Build: `make build`
- Test: `make test`
- Native CLI smoke target: `./build/chinese_chess_cli`
- Fake-engine smoke target: `make fake-engine-smoke`
- Build bundled Pikafish: `make pikafish` or `cmake --build build --target pikafish_build`
- Real-engine smoke target: `make engine-smoke`
- Frontend install: `make web-install`
- Frontend dev: `make web-dev`
- Frontend preview: `make web-preview`
- Frontend build: `make web-build` or `make web-bundle`
- WASM build: `make wasm`
- Pages-targeted frontend build: `make pages-build`

## Code Conventions

- Prefer modern C++ with clear module boundaries between rules, engine integration, and UI bridge code.
- Keep the browser-facing layer thin; reusable game logic should live in portable C++ modules.
- Internal board coordinates use `file=0..8`, `rank=0..9`, with `rank=0` at the top (black side) and `rank=9` at the bottom (red side).
- Pikafish/UCI bridge converts to algebraic squares like `a0..i9`, where `a0` is red's bottom-left corner.
- Canonical engine-facing FEN follows Pikafish's dialect, including `RNBAKABNR`/`rnbakabnr` piece letters and full six-field format.
- The current frontend stack is fixed to Vue 3 + Vite + TypeScript + Tailwind CSS + Phaser per user direction.
- The current browser/WASM boundary is defined in C++ first; Emscripten tooling is installed locally under `~/.local/emsdk`.
- Browser interaction is split: Phaser renders the board, a board-positioned DOM overlay handles direct board clicks, and a regular DOM action tray exposes selectable pieces and legal moves for automation.
- GitHub Pages deployment assumes the repository path base `/chinese-chess/`.
