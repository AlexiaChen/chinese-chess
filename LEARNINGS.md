# Project Learnings

> Append-only knowledge base maintained during issue processing.
> The agent reads this before starting each issue to avoid repeating mistakes.
> Human edits welcome — add, annotate, or mark as [OBSOLETE].

---

### L-001: [gotcha] Pikafish rejects non-native Xiangqi FEN piece letters (2026-04-29)
- **Issue**: #70 — 开发一个中国象棋
- **Trigger**: Pikafish, FEN, invalid fen, UCI, engine integration
- **Pattern**: Pikafish does not accept the earlier `h/H` horse and elephant naming style. The engine-facing FEN must use Pikafish's own dialect (`n/N`, `b/B`) and the full six-field format like `... w - - 0 1`, or `position fen ...` will be rejected.
- **Evidence**: `src/core/game.cpp:13`, `web/src/game/fen.ts:1`
- **Confidence**: 10/10
- **Action**: Whenever touching FEN generation, parsing, or browser/native engine interop, keep Pikafish's canonical dialect as the single source of truth and validate against a real engine command, not just internal parsing.

### L-002: [gotcha] select-based UCI process loops must use raw fd IO (2026-04-29)
- **Issue**: #70 — 开发一个中国象棋
- **Trigger**: UCI process, select, timeout, pipe, buffered io
- **Pattern**: Mixing `select()` with stdio-buffered reads is a trap for long-lived engine subprocesses. Readable bytes can sit inside stdio buffers and make the outer `select()` loop look idle, which shows up as fake timeouts even though the engine already answered.
- **Evidence**: `src/engine/pikafish_process.cpp:171`
- **Confidence**: 9/10
- **Action**: For any future engine or helper subprocess that is polled with `select()`, keep the transport at raw file-descriptor level (`read`/`write`) and do line buffering in project code.

### L-003: [architecture] Keep Phaser visual-only and expose move flow through DOM controls (2026-04-29)
- **Issue**: #70 — 开发一个中国象棋
- **Trigger**: Phaser, browser automation, canvas click, testability, agent-browser
- **Pattern**: Canvas rendering looked fine, but synthetic browser clicks against Phaser were not reliable enough for regression automation. The stable split was to let Phaser render the board while DOM overlays and the action tray handled piece selection and move submission through the same WASM bridge.
- **Evidence**: `web/src/game/boardScene.ts:16`, `web/src/components/PhaserBoard.vue:111`, `web/src/components/PhaserBoard.vue:265`
- **Confidence**: 9/10
- **Action**: When adding new browser-side gameplay features, preserve a semantic DOM interaction path alongside the visual canvas path so keyboard users, browser automation, and debugging all hit the same rule engine.

### L-004: [architecture] GitHub Pages AI must live inside the shared C++/WASM core (2026-04-29)
- **Issue**: #71 — 网页AI对弈的支持
- **Trigger**: GitHub Pages, WASM, web AI, Pikafish, browser engine, subprocess
- **Pattern**: The native `PikafishProcess` path depends on `fork`/`exec` and a UCI subprocess, which is not directly available in the browser runtime behind GitHub Pages. Web AI features need to be implemented inside the portable shared core and exposed through the WASM bridge instead of assuming the native engine adapter can be reused.
- **Evidence**: `src/engine/pikafish_process.cpp:36`, `src/engine/search.cpp:13`, `AGENTS.md:14`
- **Confidence**: 10/10
- **Action**: For future web AI, hint, or analysis features, extend the shared C++ core plus WASM exports first; keep native subprocess engines as native-only integrations.

### L-005: [gotcha] Browser-side search timeouts should not rely on exception-based control flow in WASM (2026-04-29)
- **Issue**: #72 — 增强AI棋力
- **Trigger**: WASM, timeout, browser AI, search budget, exception, memory access out of bounds
- **Pattern**: Native builds tolerated exception-based timeout control inside the searcher, but the browser WASM path turned that internal control flow into a runtime failure. Budgeted browser search is safer when timeout/cancel logic propagates via explicit state and return paths instead of throwing as part of normal search control.
- **Evidence**: `src/engine/search.cpp:35`, `src/engine/search.cpp:151`, `src/engine/search.cpp:307`
- **Confidence**: 10/10
- **Action**: For future WASM search budgets, cancellation, or interruption support, prefer explicit flags and bounded return paths over exception-driven control flow.

### L-006: [gotcha] New browser/WASM C exports must be added to Emscripten EXPORTED_FUNCTIONS (2026-04-30)
- **Issue**: #75 — 增加悔棋功能
- **Trigger**: WASM, Emscripten, EXPORTED_FUNCTIONS, cwrap, browser bridge, runtime abort
- **Pattern**: Adding a new C ABI function in `wasm_exports.cpp` is not enough for the browser build. If the symbol is missing from the Emscripten `EXPORTED_FUNCTIONS` list, `cwrap()` will compile cleanly but abort at runtime the first time the page calls it.
- **Evidence**: `CMakeLists.txt:62`, `src/bridge/wasm_exports.cpp:72`, `web/src/bridge/wasmBridge.ts:51`
- **Confidence**: 10/10
- **Action**: Whenever adding or renaming browser-facing C functions, update the exported symbol list in `CMakeLists.txt` and validate through a real WASM/browser flow, not native tests alone.

### L-007: [architecture] Web AI opening improvements should use an offline shared-core book before search (2026-04-30)
- **Issue**: #76 — 优化AI执先手的开局胜率
- **Trigger**: opening book, web AI, GitHub Pages, WASM, first move, repertoire
- **Pattern**: For this project, browser AI opening improvements cannot depend on live cloud queries or native-only engines. The stable path is a small offline opening book in the shared engine core that answers a few high-quality early positions first, then falls back to normal search when the line leaves the book.
- **Evidence**: `src/engine/opening_book.cpp:1`, `src/engine/search.cpp:505`, `AGENTS.md:12`
- **Confidence**: 9/10
- **Action**: When improving web opening play, extend the shared offline opening book incrementally (mainline first, repertoire later) instead of adding browser-only state or online dependencies.

### L-008: [architecture] Board-side UX fixes should flip the view, not the rules (2026-04-30)
- **Issue**: #77 — 修改一下功能和体验
- **Trigger**: board flip, perspective, black side, UX, river side, DOM overlay
- **Pattern**: When the player should control the near-side army, the correct fix is a view-only board flip that remaps rendered coordinates, river labels, highlights, and click overlays together. Swapping rule-layer red/black identity would unnecessarily ripple through AI side, turn logic, and FEN semantics.
- **Evidence**: `web/src/game/boardMetrics.ts:12`, `web/src/game/boardScene.ts:44`, `web/src/components/PhaserBoard.vue:73`, `AGENTS.md:60`
- **Confidence**: 10/10
- **Action**: For future side-orientation UX work, keep the rules/FEN stable and implement perspective changes entirely in the browser view layer.
