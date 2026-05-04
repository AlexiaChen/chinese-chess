export interface AiMoveReport {
  move: string
  score: number
  completedDepth: number
  visitedNodes: number
  elapsedMs: number
  principalVariation: string[]
  timedOut: boolean
}

export interface SearchProgress {
  currentMove: string
  depth: number
  analyzedRootMoves: number
  totalRootMoves: number
  visitedNodes: number
}

export interface PositionStatus {
  sideToMove: 'w' | 'b'
  inCheck: boolean
  hasLegalMoves: boolean
}

export interface BrowserBridge {
  currentFen: () => string
  legalMovesFrom: (square: string) => string[]
  currentPositionStatus: () => PositionStatus
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
  searchAiMoveForFenWithProgress: (
    fen: string,
    maxDepth: number,
    timeBudgetMs: number,
    onProgress: (progress: SearchProgress) => void,
  ) => AiMoveReport | null
  reset: () => void
}

interface EmscriptenRuntime {
  cwrap: <T extends (...args: never[]) => unknown>(
    ident: string,
    returnType: string | null,
    argTypes: string[],
  ) => T
  chineseChessHandleSearchProgress?: (
    currentMove: string,
    depth: number,
    analyzedRootMoves: number,
    totalRootMoves: number,
    visitedNodes: number,
  ) => void
}

type SearchProgressHookHost = typeof globalThis & {
  __chineseChessHandleSearchProgress?: (
    currentMove: string,
    depth: number,
    analyzedRootMoves: number,
    totalRootMoves: number,
    visitedNodes: number,
  ) => void
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
  const currentPositionStatus = runtime.cwrap<() => string>(
    'chinese_chess_current_position_status',
    'string',
    [],
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

  function parseAiMoveReport(raw: string) {
    if (raw.length === 0) {
      return null
    }

    const report = JSON.parse(raw) as AiMoveReport
    return report.move.length > 0 ? report : null
  }

  return {
    currentFen,
    legalMovesFrom(square: string) {
      return JSON.parse(legalMovesFrom(square)) as string[]
    },
    currentPositionStatus() {
      return JSON.parse(currentPositionStatus()) as PositionStatus
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
      return parseAiMoveReport(applyAiMoveWithReport(maxDepth, timeBudgetMs))
    },
    searchAiMoveForFenWithReport(fen: string, maxDepth: number, timeBudgetMs: number) {
      return parseAiMoveReport(searchAiMoveForFenWithReport(fen, maxDepth, timeBudgetMs))
    },
    searchAiMoveForFenWithProgress(
      fen: string,
      maxDepth: number,
      timeBudgetMs: number,
      onProgress: (progress: SearchProgress) => void,
    ) {
      const hook = (
        currentMove: string,
        depth: number,
        analyzedRootMoves: number,
        totalRootMoves: number,
        visitedNodes: number,
      ) => {
        onProgress({
          currentMove,
          depth,
          analyzedRootMoves,
          totalRootMoves,
          visitedNodes,
        })
      }
      runtime.chineseChessHandleSearchProgress = hook
      ;(globalThis as SearchProgressHookHost).__chineseChessHandleSearchProgress = hook
      try {
        return parseAiMoveReport(searchAiMoveForFenWithReport(fen, maxDepth, timeBudgetMs))
      } finally {
        delete runtime.chineseChessHandleSearchProgress
        delete (globalThis as SearchProgressHookHost).__chineseChessHandleSearchProgress
      }
    },
    reset,
  }
}
