import type { AiMoveReport } from './wasmBridge'

export interface AiSearchWorkerRequest {
  type: 'search'
  requestId: number
  fen: string
  maxDepth: number
  timeBudgetMs: number
}

export interface AiSearchWorkerResult {
  type: 'result'
  requestId: number
  report: AiMoveReport | null
}

export interface AiSearchWorkerError {
  type: 'error'
  requestId: number
  message: string
}

export type AiSearchWorkerResponse = AiSearchWorkerResult | AiSearchWorkerError
