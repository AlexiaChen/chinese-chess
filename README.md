# Chinese Chess

Chinese chess project with a shared C++ core, a native UCI bridge for Pikafish, and a browser runtime built with Vue 3, Vite, TypeScript, Tailwind CSS, and Phaser.

## Current status

- CMake workspace is set up.
- Core Xiangqi rules and move legality are implemented with automated tests.
- The project can talk to a Pikafish-compatible UCI process and parse `bestmove`.
- Official Pikafish is included as `third_party/pikafish` via git submodule.
- The web shell can already render a Xiangqi board via Vue + Phaser.
- A browser-oriented C++ bridge now exists for future WASM export (`current_fen`, `legal_moves_from`, `apply_move`, `reset`).

## Clone

```bash
git clone --recursive <your-repo-url>
cd chinese-chess
```

If you already cloned without submodules:

```bash
git submodule update --init --depth 1 third_party/pikafish
```

## Build and test

```bash
make test
```

If you prefer the raw commands, `make configure`, `make build`, and `make test` wrap:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Frontend

Install web dependencies:

```bash
make web-install
```

Start the web dev server:

```bash
make web-dev
```

Build the web bundle:

```bash
make web-build
```

Preview the production bundle locally:

```bash
make web-preview
```

Or from the CMake workspace:

```bash
make web-bundle
```

For a GitHub Pages-targeted bundle:

```bash
make pages-build
```

## Build Pikafish

The helper below builds the bundled Pikafish submodule with a conservative portable profile that worked in this environment:

```bash
make pikafish
```

You can override the target architecture if needed:

```bash
PIKAFISH_ARCH=x86-64 make pikafish
```

The same helper is also exposed as a CMake custom target:

```bash
cmake --build build --target pikafish_build
```

## Engine smoke test

```bash
make engine-smoke
```

## Build WASM bridge

```bash
make wasm
```

This emits `chinese_chess_wasm.js` and `chinese_chess_wasm.wasm` into `web/public/wasm/`.

## Browser interaction and automation

- Phaser is now the render layer only.
- The board has a DOM overlay for direct square clicks.
- The panel under the board exposes the current side's selectable pieces and legal moves as normal buttons, which makes keyboard use and browser automation stable.
- For low-level smoke checks, the page also exposes `window.__CHINESE_CHESS_TEST_API__` with `currentFen()`, `legalMovesFrom(square)`, `applyMove(move)`, and `reset()`.

## Notes

- The project currently uses Pikafish-compatible FEN and move strings (`a0..i9`, `bestmove`, `position fen ...`).
- The current web shell loads the generated WASM bridge at runtime if `web/public/wasm/` has been produced.
- CI build/test is handled by `.github/workflows/ci.yml`.
- GitHub Pages deployment is handled by `.github/workflows/pages.yml`.
