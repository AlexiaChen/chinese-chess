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
