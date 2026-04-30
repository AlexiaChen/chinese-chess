import type { AiMoveReport, SearchProgress } from './wasmBridge'

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

export interface AiSearchWorkerProgress {
  type: 'progress'
  requestId: number
  progress: SearchProgress
}

export interface AiSearchWorkerError {
  type: 'error'
  requestId: number
  message: string
}

export type AiSearchWorkerResponse = AiSearchWorkerProgress | AiSearchWorkerResult | AiSearchWorkerError
