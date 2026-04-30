# Chinese Chess — Project Knowledge Base

## Current State

- Repository now has a native CMake skeleton, a portable rules core, a CLI target, and a test target.
- The active goal is to build a Chinese chess game with human-vs-AI play, local execution, and a web build deployable to GitHub Pages.

## Architecture

- Shared C++ core built with CMake.
- Current implemented modules:
  - `src/core/game.h/.cpp`: board state, FEN parsing, legal move generation, move application, and check detection
  - `src/engine/opening_book.h/.cpp`: offline opening-book lines for early-game AI move selection before full search
  - `src/engine/uci_codec.h/.cpp`: bridge between internal coordinates and Pikafish-style UCI square/move strings
- `src/engine/search.h/.cpp`: portable Xiangqi search with iterative deepening, aspiration windows, root PVS, null-move pruning, move ordering, transposition caching, quiescence search, a feature-based evaluator (including public `evaluate_position()` and Xiangqi line-pressure scoring), plus shallow AI-side search progress callbacks for native/WASM gameplay
  - `src/engine/pikafish_process.h/.cpp`: native UCI subprocess adapter for a Pikafish-compatible engine command
- `src/bridge/browser_session.h/.cpp`: browser-facing session wrapper over the core rules engine, including move history for browser-side undo plus side-effect-free AI search from an arbitrary FEN snapshot
- `src/bridge/wasm_exports.cpp`: C ABI surface exported to the browser/WASM runtime (`current_fen`, `legal_moves_from`, `apply_move`, `undo_last_move`, `undo_count`, `apply_ai_move`, `apply_ai_move_with_limits`, `apply_ai_move_with_report`, `search_ai_move_for_fen_with_report`, `reset`)
  - `src/apps/cli/main.cpp`: native CLI target that can print the board or query a configured engine command for `bestmove`
  - `tests/game_tests.cpp`: rules and codec coverage
  - `tests/fixtures/fake_uci_engine.py`: fake UCI engine used for adapter tests
  - `third_party/pikafish`: official Pikafish source as a git submodule
- `web/`: Vue 3 + Vite + TypeScript + Tailwind CSS + Phaser frontend, with Phaser as the render layer and DOM controls for interaction/testability
  - `web/src/App.vue`: player-facing shell for opening-side selection, AI turn orchestration, undo controls, summarized AI insight cards (last move, eval, depth, nodes, elapsed time, PV), live AI-thinking square state, and the current depth-20 / 2000ms browser AI defaults
  - `web/src/workers/aiSearchWorker.ts`: dedicated Web Worker that loads the WASM bridge, searches from FEN snapshots off the main thread, streams progress updates, and returns summarized AI reports to the Vue shell
  - `web/src/components/PhaserBoard.vue`: Phaser board wrapper with DOM piece/move overlays, a green AI-thinking square overlay, and SVG path highlighting for the AI's latest move
  - `.github/workflows/ci.yml`: GitHub Actions CI for native tests plus WASM/frontend builds
  - `.github/workflows/pages.yml`: GitHub Pages build and deploy workflow
- Target architecture remains:
  - native targets for engine/testing on WSL2
  - a WebAssembly target for browser play
  - a thin browser UI where Phaser renders the board, DOM controls drive moves, and a built-in WASM searcher can answer as the web AI opponent

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
- Board presentation is view-only and can flip automatically with the player's side so the human-controlled army stays on the near side while rules/FEN remain unchanged.
- GitHub Pages-compatible web AI must stay inside the shared C++/WASM core; native-only subprocess engines like `PikafishProcess` cannot be used directly in the browser runtime.
- Browser AI strength now depends on `SearchOptions`-style limits (max depth plus time budget) rather than a fixed-depth-only search contract.
- Default browser AI tuning currently uses `max depth = 20` with a `2000ms` budget; future strength work should prefer making that budget more selective before pushing browser waits much higher.
- The Vue shell owns the live BrowserSession state; Worker-based AI searches must operate on FEN snapshots and return moves/reports that the main thread applies to the authoritative session.
- Worker-based browser AI can also stream search-progress focus squares from the shared WASM searcher; the main thread should treat those as transient UI hints and clear them on cancel, undo, reset, or result application.
- Browser/native AI can short-circuit into the shared offline opening book for early red-side mainline moves before falling back to search.
- The current evaluator stays in the handcrafted / feature-based family; when extending it, prefer Xiangqi-specific pressure/activity signals that keep the shared WASM core lightweight before attempting NNUE-scale complexity.
- Player-facing AI feedback should use summarized search reports from the WASM bridge (last move, eval, completed depth, visited nodes, elapsed time, PV) instead of streaming every explored node to the UI.
- GitHub Pages deployment assumes the repository path base `/chinese-chess/`.
