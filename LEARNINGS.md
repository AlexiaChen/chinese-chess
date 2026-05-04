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

### L-009: [gotcha] Browser AI budget increases should be benchmarked against search efficiency, not just raised blindly (2026-04-30)
- **Issue**: #78 — 强化AI智能和棋力
- **Trigger**: browser AI, WASM, time budget, blocking, performance, search depth
- **Pattern**: In this repo the browser AI search runs synchronously on the UI thread. Raising the default budget can increase wait time without proportionate strength gains; a sampled midgame went from 2000ms to 5000ms without completing an extra depth, while search-efficiency improvements materially reduced node count.
- **Evidence**: `web/src/App.vue:161`, `tests/game_tests.cpp:219`, `src/engine/search.cpp:340`
- **Confidence**: 9/10
- **Action**: Before increasing browser AI think time again, benchmark whether the extra budget actually buys more completed depth or stronger choices; prefer search-efficiency improvements first.

### L-010: [gotcha] Xiangqi null-move pruning should stay conservative and use verification search (2026-04-30)
- **Issue**: #78 — 强化AI智能和棋力
- **Trigger**: null move pruning, search heuristic, endgame, verification search, zugzwang
- **Pattern**: Direct null-move pruning is risky in low-material Xiangqi positions. In this codebase it is safer to gate the heuristic on remaining material and verify fail-high results before accepting the cutoff.
- **Evidence**: `src/engine/search.cpp:154`, `src/engine/search.cpp:374`
- **Confidence**: 8/10
- **Action**: When tuning null-move pruning here, treat low-material positions cautiously and keep verification search in the loop instead of enabling broad unconditional cutoffs.

### L-011: [architecture] Worker-based browser AI should search from FEN snapshots, not own the authoritative session (2026-04-30)
- **Issue**: #79 — 增加异步Worker搜索
- **Trigger**: Web Worker, browser AI, WASM, FEN snapshot, session state, async search
- **Pattern**: For this project the stable Worker split is: main thread keeps the authoritative `BrowserSession`, while the Worker receives a FEN snapshot, runs search off-thread, and returns a move/report for the main thread to apply. Letting the Worker own the live session would complicate undo/reset/state sync.
- **Evidence**: `web/src/App.vue:246`, `web/src/workers/aiSearchWorker.ts:16`, `src/bridge/browser_session.cpp:38`
- **Confidence**: 10/10
- **Action**: For future async AI or analysis features, keep state mutation on the main thread and make Worker search APIs pure over serialized inputs like FEN.

### L-012: [gotcha] Browser WASM search cancellation can start with Worker termination before deeper engine-level interrupts exist (2026-04-30)
- **Issue**: #79 — 增加异步Worker搜索
- **Trigger**: Worker cancellation, async search, WASM, reset, stale result, terminate
- **Pattern**: A full engine-level cancel signal would be nicer, but a reliable first step is to terminate the in-flight Worker and ignore stale replies. That is enough to unblock UI architecture work before adding internal search interruption support.
- **Evidence**: `web/src/App.vue:162`, `web/src/App.vue:241`, `web/src/workers/aiSearchWorker.ts:16`
- **Confidence**: 8/10
- **Action**: When adding async search here, ship Worker lifecycle cancellation first; add fine-grained search interrupts only after the message flow is stable.

### L-013: [gotcha] Worker-side WASM builds must include `worker` in Emscripten ENVIRONMENT (2026-04-30)
- **Issue**: #79 — 增加异步Worker搜索
- **Trigger**: Emscripten, Web Worker, WASM runtime, ENVIRONMENT, browser AI, worker load failure
- **Pattern**: A browser-facing WASM bundle can still fail inside a Worker if Emscripten is compiled with `-sENVIRONMENT=web` only. For this project, Worker-based AI search requires `-sENVIRONMENT=web,worker` so the runtime is valid in both contexts.
- **Evidence**: `CMakeLists.txt:61`, reproduced through `web preview` + browser automation after `a3a4`
- **Confidence**: 10/10
- **Action**: Whenever adding Worker-hosted WASM features here, verify the Emscripten environment flags include `worker` and validate through an actual preview/browser run, not just builds.

### L-014: [architecture] For this project, feature-based eval should land before NNUE-scale complexity (2026-04-30)
- **Issue**: #80 — 增强AI的评估函数
- **Trigger**: evaluation function, feature-based eval, handcrafted eval, NNUE, WASM, GitHub Pages
- **Pattern**: Frontier Xiangqi engines lean toward NNUE or hybrid evaluation, but this project's shared WASM core benefits more from first deepening the handcrafted / feature-based evaluator. That path keeps GitHub Pages deployment simple while still improving the engine meaningfully.
- **Evidence**: issue #80 research summary, `AGENTS.md`, `src/engine/search.cpp`
- **Confidence**: 9/10
- **Action**: When planning future eval work here, treat lightweight feature-based terms as the default next step and only move to small NNUE/hybrid once the handcrafted feature stack is mature.

### L-015: [gotcha] Exposing the evaluator as a public testable API helps de-risk search tuning (2026-04-30)
- **Issue**: #80 — 增强AI的评估函数
- **Trigger**: evaluate_position, search tuning, regression tests, evaluation API
- **Pattern**: Search tuning is hard to verify when evaluation stays buried inside the searcher. In this codebase, exposing `evaluate_position()` made it possible to lock a Xiangqi-specific pressure signal with a focused regression test instead of inferring it indirectly from move choices.
- **Evidence**: `src/engine/search.h:26`, `tests/game_tests.cpp:236`
- **Confidence**: 9/10
- **Action**: For future evaluator work here, prefer direct evaluator assertions when possible so heuristic changes stay reviewable and regressions are easier to catch.

### L-016: [architecture] Browser AI thinking UI should stream real search progress from the shared WASM core (2026-04-30)
- **Issue**: #81 — 优化界面
- **Trigger**: worker, wasm, AI thinking, progress, highlight
- **Pattern**: In this project, a truthful “AI is analyzing this piece” indicator works best when the shared searcher emits progress directly and the Worker forwards it to the main-thread UI. Frontend-only fake animation would drift away from the real search path and become misleading as search limits or move ordering change.
- **Evidence**: `src/engine/search.cpp:15`, `web/src/workers/aiSearchWorker.ts:24`, `web/src/App.vue:190`, `web/src/components/PhaserBoard.vue:179`
- **Confidence**: 9/10
- **Action**: For future browser AI analysis or hint UI, extend the shared WASM search path to publish progress first, then render transient UI from those messages instead of inventing client-only thinking animations.

### L-017: [gotcha] Shared C++ browser-AI changes require rebuilding WASM, not just the web bundle (2026-04-30)
- **Issue**: #81 — 优化界面
- **Trigger**: wasm, web-build, stale runtime, preview, browser AI
- **Pattern**: Updating Vue/Worker code alone does not refresh browser AI behavior when the real change lives in the shared C++ searcher or bridge layer. The page can keep loading an older WASM runtime unless the dedicated WASM build step is rerun alongside the frontend bundle.
- **Evidence**: `AGENTS.md:48`, `AGENTS.md:49`, `web/src/bridge/wasmBridge.ts:71`, `src/engine/search.cpp:15`
- **Confidence**: 10/10
- **Action**: Whenever changing shared browser AI logic in C++ or the WASM bridge, run `make wasm` before or alongside `make web-build`, then validate against a fresh preview session.

### L-018: [gotcha] Xiangqi river-width feedback usually points at board spacing before label typography (2026-05-04)
- **Issue**: #82 — 优化棋盘UI
- **Trigger**: river, 楚河汉界, board spacing, river gap, board UI
- **Pattern**: On this board, the correct fix for an oversized `楚河/汉界` area was to shrink the river band itself back to normal cell spacing and only then tune the lettering. Changing glyph size first made the UI worse because the real problem lived in board geometry, not typography.
- **Evidence**: `web/src/game/boardMetrics.ts:1`, `web/src/game/boardScene.ts:163`
- **Confidence**: 9/10
- **Action**: When future board-geometry feedback says `楚河/汉界 too wide`, inspect river-band metrics before touching label typography.
