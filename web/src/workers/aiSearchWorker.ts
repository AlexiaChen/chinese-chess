/// <reference lib="webworker" />

import type { AiSearchWorkerRequest, AiSearchWorkerResponse } from '../bridge/aiSearchWorkerProtocol'
import type { BrowserBridge } from '../bridge/wasmBridge'
import { createWasmBridge } from '../bridge/wasmBridge'

declare const self: DedicatedWorkerGlobalScope

let bridgePromise: Promise<BrowserBridge> | null = null

function getBridge() {
  bridgePromise ??= createWasmBridge()
  return bridgePromise
}

self.onmessage = async (event: MessageEvent<AiSearchWorkerRequest>) => {
  const message = event.data
  if (message.type !== 'search') {
    return
  }

  try {
    const bridge = await getBridge()
    const report = bridge.searchAiMoveForFenWithProgress(
      message.fen,
      message.maxDepth,
      message.timeBudgetMs,
      (progress) => {
        const response: AiSearchWorkerResponse = {
          type: 'progress',
          requestId: message.requestId,
          progress,
        }
        self.postMessage(response)
      },
    )
    const response: AiSearchWorkerResponse = {
      type: 'result',
      requestId: message.requestId,
      report,
    }
    self.postMessage(response)
  } catch (error) {
    const response: AiSearchWorkerResponse = {
      type: 'error',
      requestId: message.requestId,
      message: error instanceof Error ? error.message : 'AI Worker 搜索失败',
    }
    self.postMessage(response)
  }
}

export {}
