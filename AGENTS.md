# Chinese Chess — Project Knowledge Base

## Current State

- Repository now has a native CMake skeleton, a portable rules core, a CLI target, and a test target.
- The active goal is to build a Chinese chess game with human-vs-AI play, local execution, and a web build deployable to GitHub Pages.
- The browser NNUE path is now e2e-working again: after a human move, the AI can search in shared WASM and auto-apply its reply instead of failing mid-search.

## Architecture

- Shared C++ core built with CMake.
- Current implemented modules:
  - `src/core/game.h/.cpp`: board state, FEN parsing, legal move generation, move application, and check detection
  - `src/engine/opening_book.h/.cpp`: offline opening-book lines for early-game AI move selection before full search
  - `src/engine/uci_codec.h/.cpp`: bridge between internal coordinates and Pikafish-style UCI square/move strings
- `src/engine/search.h/.cpp`: portable Xiangqi search with iterative deepening, aspiration windows, root PVS, null-move pruning, move ordering, transposition caching, quiescence search with delta pruning, Pikafish-NNUE-backed static evaluation in normal positions, and shallow AI-side search progress callbacks for native/WASM gameplay
  - `src/engine/pikafish_nnue.h/.cpp`: single-threaded adapter that loads `third_party/pikafish/src/pikafish.nnue`, converts `GameState` FEN into a Pikafish `Position`, runs Pikafish NNUE inference inside the shared core, falls back to the legacy handcrafted evaluator when a position is unsupported by Pikafish's validator, and reuses heap-backed Pikafish workspaces so deeper WASM searches do not blow the browser stack
  - `src/engine/pikafish_process.h/.cpp`: native UCI subprocess adapter for a Pikafish-compatible engine command
- `src/bridge/browser_session.h/.cpp`: browser-facing session wrapper over the core rules engine, including move history for browser-side undo, current-position status reporting (check / legal-move availability), plus side-effect-free AI search from an arbitrary FEN snapshot
- `src/bridge/wasm_exports.cpp`: C ABI surface exported to the browser/WASM runtime (`current_fen`, `legal_moves_from`, `current_position_status`, `apply_move`, `undo_last_move`, `undo_count`, `apply_ai_move`, `apply_ai_move_with_limits`, `apply_ai_move_with_report`, `search_ai_move_for_fen_with_report`, `reset`)
  - `src/apps/cli/main.cpp`: native CLI target that can print the board or query a configured engine command for `bestmove`
- `tests/game_tests.cpp`: rules, codec, evaluator regression, and node-budget coverage
  - `tests/fixtures/fake_uci_engine.py`: fake UCI engine used for adapter tests
  - `third_party/pikafish`: official Pikafish source as a git submodule
- `web/`: Vue 3 + Vite + TypeScript + Tailwind CSS + Phaser frontend, with Phaser as the render layer and DOM controls for interaction/testability
  - `web/src/App.vue`: player-facing shell for opening-side selection, AI turn orchestration, undo controls, summarized AI insight cards (last move, eval, depth, nodes, elapsed time, PV), short-lived battle notifications for capture/check/mate, live AI-thinking square state, and the current depth-20 / 5000ms browser AI defaults
  - `web/src/workers/aiSearchWorker.ts`: dedicated Web Worker that loads the shared search WASM bridge, searches from FEN snapshots off the main thread, and streams summarized progress back to Vue
  - `web/src/components/PhaserBoard.vue`: Phaser board wrapper with DOM piece/move overlays, a green AI-thinking square overlay, SVG path highlighting for the AI's latest move, and structured move-applied events carrying pre/post-move FEN for the Vue shell
  - `web/src/game/boardScene.ts`: Phaser scene that draws the square-cell Xiangqi board, a normal-width river band with horizontal `楚河 / 汉界` labels, traditional soldier/cannon position markers, and the rendered pieces
  - `.github/workflows/ci.yml`: GitHub Actions CI for native tests plus WASM/frontend builds
  - `.github/workflows/pages.yml`: GitHub Pages build and deploy workflow
- Target architecture remains:
  - native targets for engine/testing on WSL2
  - a WebAssembly target for browser play
  - a thin browser UI where Phaser renders the board, DOM controls drive moves, and a built-in WASM searcher can answer as the web AI opponent

## Build & Run

- Build system target: CMake.
- Default command entrypoint: top-level `Makefile`
- Debug configure/build/test defaults: `make configure`, `make build`, `make test`
- Explicit Debug aliases: `make configure-debug`, `make build-debug`, `make test-debug`
- Explicit Release aliases: `make configure-release`, `make build-release`, `make test-release`
- Native CLI smoke targets: `make native-cli` (Debug) or `make native-cli-release`
- Fake-engine smoke target: `make fake-engine-smoke`
- Release fake-engine smoke target: `make fake-engine-smoke-release`
- Build bundled Pikafish: `make pikafish` or `cmake --build build/debug --target pikafish_build`
- Real-engine smoke target: `make engine-smoke`
- Release real-engine smoke target: `make engine-smoke-release`
- Frontend install: `make web-install`
- Frontend dev: `make web-dev`
- Frontend preview: `make web-preview`
- Frontend build: `make web-build` or `make web-bundle`
- WASM build: `make wasm`/`make wasm-debug` for Debug, `make wasm-release` for Release (emits `chinese_chess_wasm.js/.wasm/.data`; the `.data` payload carries `pikafish.nnue` for the shared NNUE evaluator)
- Pages-targeted frontend build: `make pages-build` (always builds Release WASM first)

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
- Keep the river band aligned with normal cell spacing; if `楚河 / 汉界` looks too large, inspect board geometry before shrinking the label glyphs.
- GitHub Pages-compatible web AI must stay inside the shared C++/WASM core; native-only subprocess engines like `PikafishProcess` cannot be used directly in the browser runtime.
- Full Pikafish `Engine` search must not be compiled into the browser runtime; the viable browser path is the shared single-threaded searcher plus the adapted Pikafish NNUE evaluation code.
- Browser AI strength now depends on `SearchOptions`-style limits (max depth plus time budget) rather than a fixed-depth-only search contract.
- Default browser AI tuning currently uses `max depth = 20` with a `5000ms` budget; future strength work should prefer making that budget more selective before pushing browser waits much higher.
- The Vue shell owns the live BrowserSession state; Worker-based AI searches must operate on FEN snapshots and return moves/reports that the main thread applies to the authoritative session.
- Worker-based browser AI can also stream search-progress focus squares from the shared WASM searcher; the main thread should treat those as transient UI hints and clear them on cancel, undo, reset, or result application.
- Browser/native AI can short-circuit into the shared offline opening book for early red-side mainline moves before falling back to search.
- The primary evaluator now routes through Pikafish NNUE when the position can be represented by Pikafish; positions that are in-check or rejected by Pikafish's position validator fall back to the legacy handcrafted evaluator to keep search/test coverage intact.
- In browser/WASM, keep large Pikafish NNUE temporaries off the stack; reuse heap-backed `Position`, `StateInfo`, `AccumulatorStack`, and `AccumulatorCaches` inside the adapter or deeper searches can fail with `memory access out of bounds`.
- When validating browser AI, distinguish opening-book plies from true search: `AI先（AI执红）` can auto-move with `completed depth = 0` and `visited nodes = 0`, so use a post-opening human move such as `a3a4` to confirm the real NNUE-backed search path and automatic AI reply.
- Strength tuning should add direct evaluator regressions and preserve the midgame node-budget guardrail in the same change, so heuristic gains do not silently bloat search cost.
- Player-facing AI feedback should use summarized search reports from the WASM bridge (last move, eval, completed depth, visited nodes, elapsed time, PV) instead of streaming every explored node to the UI.
- GitHub Pages deployment assumes the repository path base `/chinese-chess/`.
