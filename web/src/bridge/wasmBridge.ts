export interface BrowserBridge {
  currentFen: () => string
  legalMovesFrom: (square: string) => string[]
  applyMove: (move: string) => boolean
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
  const reset = runtime.cwrap<() => void>('chinese_chess_reset', null, [])

  return {
    currentFen,
    legalMovesFrom(square: string) {
      return JSON.parse(legalMovesFrom(square)) as string[]
    },
    applyMove(move: string) {
      return applyMove(move) === 1
    },
    reset,
  }
}
