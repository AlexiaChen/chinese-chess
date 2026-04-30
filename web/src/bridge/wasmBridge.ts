export interface AiMoveReport {
  move: string
  score: number
  completedDepth: number
  visitedNodes: number
  elapsedMs: number
  principalVariation: string[]
  timedOut: boolean
}

export interface BrowserBridge {
  currentFen: () => string
  legalMovesFrom: (square: string) => string[]
  applyMove: (move: string) => boolean
  undoLastMove: () => boolean
  undoCount: () => number
  applyAiMove: (maxDepth: number, timeBudgetMs?: number) => string | null
  applyAiMoveWithReport: (maxDepth: number, timeBudgetMs: number) => AiMoveReport | null
  searchAiMoveForFenWithReport: (
    fen: string,
    maxDepth: number,
    timeBudgetMs: number,
  ) => AiMoveReport | null
  reset: () => void
}

interface EmscriptenRuntime {
  cwrap: <T extends (...args: never[]) => unknown>(
    ident: string,
    returnType: string | null,
    argTypes: string[],
  ) => T
}

interface EmscriptenModuleFactory {
  default: () => Promise<EmscriptenRuntime>
}

export async function createWasmBridge(): Promise<BrowserBridge> {
  const wasmEntrypoint = `${import.meta.env.BASE_URL}wasm/chinese_chess_wasm.js`
  const moduleFactory = (await import(
    /* @vite-ignore */ wasmEntrypoint
  )) as EmscriptenModuleFactory

  const runtime = await moduleFactory.default()

  const currentFen = runtime.cwrap<() => string>('chinese_chess_current_fen', 'string', [])
  const legalMovesFrom = runtime.cwrap<(square: string) => string>(
    'chinese_chess_legal_moves_from',
    'string',
    ['string'],
  )
  const applyMove = runtime.cwrap<(move: string) => number>('chinese_chess_apply_move', 'number', [
    'string',
  ])
  const undoLastMove = runtime.cwrap<() => number>('chinese_chess_undo_last_move', 'number', [])
  const undoCount = runtime.cwrap<() => number>('chinese_chess_undo_count', 'number', [])
  const applyAiMove = runtime.cwrap<(depth: number) => string>('chinese_chess_apply_ai_move', 'string', [
    'number',
  ])
  const applyAiMoveWithLimits = runtime.cwrap<(maxDepth: number, timeBudgetMs: number) => string>(
    'chinese_chess_apply_ai_move_with_limits',
    'string',
    ['number', 'number'],
  )
  const applyAiMoveWithReport = runtime.cwrap<(maxDepth: number, timeBudgetMs: number) => string>(
    'chinese_chess_apply_ai_move_with_report',
    'string',
    ['number', 'number'],
  )
  const searchAiMoveForFenWithReport = runtime.cwrap<
    (fen: string, maxDepth: number, timeBudgetMs: number) => string
  >('chinese_chess_search_ai_move_for_fen_with_report', 'string', ['string', 'number', 'number'])
  const reset = runtime.cwrap<() => void>('chinese_chess_reset', null, [])

  return {
    currentFen,
    legalMovesFrom(square: string) {
      return JSON.parse(legalMovesFrom(square)) as string[]
    },
    applyMove(move: string) {
      return applyMove(move) === 1
    },
    undoLastMove() {
      return undoLastMove() === 1
    },
    undoCount,
    applyAiMove(maxDepth: number, timeBudgetMs?: number) {
      const move =
        timeBudgetMs === undefined ? applyAiMove(maxDepth) : applyAiMoveWithLimits(maxDepth, timeBudgetMs)
      return move.length > 0 ? move : null
    },
    applyAiMoveWithReport(maxDepth: number, timeBudgetMs: number) {
      const raw = applyAiMoveWithReport(maxDepth, timeBudgetMs)
      if (raw.length === 0) {
        return null
      }

      const report = JSON.parse(raw) as AiMoveReport
      return report.move.length > 0 ? report : null
    },
    searchAiMoveForFenWithReport(fen: string, maxDepth: number, timeBudgetMs: number) {
      const raw = searchAiMoveForFenWithReport(fen, maxDepth, timeBudgetMs)
      if (raw.length === 0) {
        return null
      }

      const report = JSON.parse(raw) as AiMoveReport
      return report.move.length > 0 ? report : null
    },
    reset,
  }
}
