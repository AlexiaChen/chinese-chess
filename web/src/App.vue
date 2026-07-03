<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch, watchEffect } from 'vue'

import type { AiSearchWorkerRequest, AiSearchWorkerResponse } from './bridge/aiSearchWorkerProtocol'
import type { AiMoveReport, BrowserBridge, PositionStatus } from './bridge/wasmBridge'
import { createWasmBridge } from './bridge/wasmBridge'
import PhaserBoard from './components/PhaserBoard.vue'
import { INITIAL_FEN, parseFen, pieceDisplayName, sideLabel } from './game/fen'

const currentFen = ref(INITIAL_FEN)
const resetKey = ref(0)
const bridge = ref<BrowserBridge | null>(null)
const bridgeStatus = ref('棋盘正在加载，请稍候…')
const aiEnabled = ref(true)
const aiThinking = ref(false)
const aiThinkingMove = ref<string | null>(null)
const humanSide = ref<'w' | 'b'>('w')
const lastAiReport = ref<AiMoveReport | null>(null)
const lastAiMoveLabel = ref('')
const undoCount = ref(0)
const battleNotification = ref<BattleNotification | null>(null)

type AiDifficultyKey = 'rookie' | 'intermediate' | 'hard' | 'master' | 'grandmaster'

type AiDifficultyPreset = {
  key: AiDifficultyKey
  label: string
  maxDepth: number
  timeBudgetMs: number
  summary: string
}

const AI_DIFFICULTY_PRESETS: AiDifficultyPreset[] = [
  {
    key: 'rookie',
    label: '菜鸟',
    maxDepth: 6,
    timeBudgetMs: 1000,
    summary: '接近秒回，适合轻松体验。',
  },
  {
    key: 'intermediate',
    label: '中级',
    maxDepth: 8,
    timeBudgetMs: 2000,
    summary: '会给一些压力，等待仍然可接受。',
  },
  {
    key: 'hard',
    label: '高难',
    maxDepth: 12,
    timeBudgetMs: 3500,
    summary: '默认档，偏重棋力但还不至于太磨人。',
  },
  {
    key: 'master',
    label: '大师',
    maxDepth: 16,
    timeBudgetMs: 8000,
    summary: '明显更强，适合愿意等待更久的玩家。',
  },
  {
    key: 'grandmaster',
    label: '特级大师',
    maxDepth: 20,
    timeBudgetMs: 15000,
    summary: '保留最高搜索上限，单步思考时间最长。',
  },
]

const aiDifficulty = ref<AiDifficultyKey>('hard')

type AppliedMovePayload = {
  move: string
  fenBefore: string
  fenAfter: string
}

type BattleNotificationTone = 'capture' | 'check' | 'mate'

type BattleNotification = {
  id: number
  title: string
  detail: string
  tone: BattleNotificationTone
  tags: string[]
}

type PendingAiSearch = {
  requestId: number
  resolve: (report: AiMoveReport | null) => void
  reject: (error: Error) => void
}

let aiWorker: Worker | null = null
let nextAiSearchRequestId = 0
let pendingAiSearch: PendingAiSearch | null = null
let nextBattleNotificationId = 0
let battleNotificationTimer: number | null = null

declare global {
  interface Window {
    __CHINESE_CHESS_TEST_API__?: {
      applyMove: (move: string) => boolean
      undoLastMove: () => boolean
      undoCount: () => number
      applyAiMove: (maxDepth: number, timeBudgetMs?: number) => string | null
      applyAiMoveWithReport: (maxDepth: number, timeBudgetMs: number) => AiMoveReport | null
      currentFen: () => string
      legalMovesFrom: (square: string) => string[]
      setAiEnabled: (enabled: boolean) => void
      setAiDifficulty: (difficulty: AiDifficultyKey) => void
      setHumanSide: (side: 'w' | 'b') => void
      reset: () => void
    }
  }
}

function formatAiTimeBudget(timeBudgetMs: number) {
  const seconds = timeBudgetMs / 1000
  return Number.isInteger(seconds) ? `${seconds} 秒` : `${seconds.toFixed(1)} 秒`
}

const activeSideToken = computed(() => currentFen.value.split(' ')[1] ?? 'w')
const activeSide = computed(() => sideLabel(activeSideToken.value))
const aiSide = computed(() => (humanSide.value === 'w' ? 'b' : 'w'))
const selectedAiPreset = computed(
  () => AI_DIFFICULTY_PRESETS.find((preset) => preset.key === aiDifficulty.value) ?? AI_DIFFICULTY_PRESETS[2],
)
const aiMaxDepth = computed(() => selectedAiPreset.value.maxDepth)
const aiTimeBudgetMs = computed(() => selectedAiPreset.value.timeBudgetMs)
const aiTimeBudgetLabel = computed(() => formatAiTimeBudget(aiTimeBudgetMs.value))
const humanTurn = computed(() => activeSideToken.value === humanSide.value)
const interactionLocked = computed(() => aiThinking.value || (aiEnabled.value && !humanTurn.value))
const canUndo = computed(() => {
  if (!bridge.value || aiThinking.value) {
    return false
  }

  return aiEnabled.value ? undoCount.value >= 2 : undoCount.value >= 1
})
const modeLabel = computed(() => {
  if (!aiEnabled.value) {
    return '双人对弈'
  }

  return humanSide.value === 'w' ? '我执红，AI 执黑' : 'AI 执红，我执黑'
})
const aiEvalLabel = computed(() => {
  if (!lastAiReport.value) {
    return '—'
  }

  const score = lastAiReport.value.score / 100
  return `${score >= 0 ? '+' : ''}${score.toFixed(2)}`
})
const aiPvLabel = computed(() =>
  lastAiReport.value?.principalVariation.length
    ? lastAiReport.value.principalVariation.join(' ')
    : '等待 AI 落子后显示',
)
const undoButtonLabel = computed(() => (aiEnabled.value ? '悔一回合' : '悔一步'))
const undoAvailabilityLabel = computed(() => {
  if (aiEnabled.value) {
    return undoCount.value >= 2 ? `可回退 ${Math.floor(undoCount.value / 2)} 个回合` : '至少完成一回合后可用'
  }

  return undoCount.value > 0 ? `可回退 ${undoCount.value} 步` : '暂无可撤销着法'
})
const undoHint = computed(() =>
  aiEnabled.value ? '会同时撤回你的上一步与 AI 的应手。' : '仅撤销最近一步落子。',
)
const setupTips: string[] = [
  '支持选择我先或 AI 先，开局会自动按选定方落子。',
  '左侧 5 档 AI 难度会改变下一回合的搜索预算；高档位更强，但等待也更久。',
  '棋盘会自动让玩家一方保持在下方，避免操作河对面的棋子。',
  'AI 思考时会用绿色方框跳动标出当前正在分析的起手棋子，落子后继续保留上一步路径与搜索摘要。',
  '关闭 AI 后仍可作为双人对弈棋盘使用。',
]

function describeAiMove(fenBeforeMove: string, move: string): string {
  const fromSquare = move.slice(0, 2)
  const toSquare = move.slice(2, 4)
  const position = parseFen(fenBeforeMove)
  const file = fromSquare.charCodeAt(0) - 'a'.charCodeAt(0)
  const rank = 9 - Number(fromSquare[1])
  const piece = position.board[rank * 9 + file]

  if (!piece) {
    return `${fromSquare} → ${toSquare}`
  }

  const pieceSide = piece === piece.toUpperCase() ? '红方' : '黑方'
  return `${pieceSide}${pieceDisplayName(piece)} ${fromSquare} → ${toSquare}`
}

function refreshBridgeStatus() {
  if (!bridge.value) {
    bridgeStatus.value = '棋盘正在加载，请稍候…'
    return
  }

  if (aiThinking.value) {
    bridgeStatus.value = `AI 正在思考（${selectedAiPreset.value.label}，深度上限 ${aiMaxDepth.value}，最长 ${aiTimeBudgetLabel.value}）…`
    return
  }

  if (!aiEnabled.value) {
    bridgeStatus.value = '棋盘已准备好，可双人对弈。'
    return
  }

  if (activeSideToken.value === aiSide.value) {
    bridgeStatus.value = humanSide.value === 'w' ? '轮到 AI 行棋。' : 'AI 正在准备先手。'
    return
  }

  if (lastAiMoveLabel.value) {
    bridgeStatus.value = `AI 刚走 ${lastAiMoveLabel.value}`
    return
  }

  bridgeStatus.value = humanSide.value === 'w' ? '棋盘已准备好，你执红先行。' : '棋盘已准备好，AI 执红先行。'
}

function clearAiInsight() {
  lastAiReport.value = null
  lastAiMoveLabel.value = ''
}

function clearAiThinkingProgress() {
  aiThinkingMove.value = null
}

function clearBattleNotification() {
  if (battleNotificationTimer !== null) {
    window.clearTimeout(battleNotificationTimer)
    battleNotificationTimer = null
  }
  battleNotification.value = null
}

function showBattleNotification(notification: Omit<BattleNotification, 'id'>) {
  clearBattleNotification()
  battleNotification.value = {
    id: ++nextBattleNotificationId,
    ...notification,
  }
  battleNotificationTimer = window.setTimeout(() => {
    battleNotification.value = null
    battleNotificationTimer = null
  }, 2200)
}

function syncFromBridge() {
  if (!bridge.value) {
    currentFen.value = INITIAL_FEN
    undoCount.value = 0
    return
  }

  currentFen.value = bridge.value.currentFen()
  undoCount.value = bridge.value.undoCount()
}

function disposeAiWorker(resolvePendingWithNull = false) {
  if (pendingAiSearch) {
    const pending = pendingAiSearch
    pendingAiSearch = null
    if (resolvePendingWithNull) {
      pending.resolve(null)
    }
  }

  if (aiWorker) {
    aiWorker.terminate()
    aiWorker = null
  }
}

function squareToBoardIndex(square: string) {
  const file = square.charCodeAt(0) - 'a'.charCodeAt(0)
  const rank = 9 - Number(square[1])
  return rank * 9 + file
}

function pieceAtSquare(fen: string, square: string) {
  const position = parseFen(fen)
  return position.board[squareToBoardIndex(square)]
}

function pieceSideLabel(piece: string) {
  return piece === piece.toUpperCase() ? '红方' : '黑方'
}

function battleNotificationFrameClass(tone: BattleNotificationTone) {
  switch (tone) {
    case 'mate':
      return 'border-rose-300/30 bg-[linear-gradient(135deg,rgba(125,24,37,0.9),rgba(43,8,16,0.94))] shadow-[0_18px_50px_rgba(81,12,25,0.35)]'
    case 'check':
      return 'border-amber-200/30 bg-[linear-gradient(135deg,rgba(138,52,23,0.9),rgba(51,22,10,0.94))] shadow-[0_18px_50px_rgba(112,45,16,0.35)]'
    case 'capture':
      return 'border-emerald-200/25 bg-[linear-gradient(135deg,rgba(46,78,64,0.9),rgba(14,24,22,0.94))] shadow-[0_18px_50px_rgba(10,30,24,0.35)]'
  }
}

function battleNotificationTitleClass(tone: BattleNotificationTone) {
  switch (tone) {
    case 'mate':
      return 'border border-rose-200/35 bg-rose-200/12 text-rose-50'
    case 'check':
      return 'border border-amber-200/35 bg-amber-200/12 text-amber-50'
    case 'capture':
      return 'border border-emerald-200/35 bg-emerald-200/12 text-emerald-50'
  }
}

function battleNotificationTagClass(tone: BattleNotificationTone) {
  switch (tone) {
    case 'mate':
      return 'border border-white/12 bg-white/10 text-rose-100/90'
    case 'check':
      return 'border border-white/12 bg-white/10 text-amber-100/90'
    case 'capture':
      return 'border border-white/12 bg-white/10 text-emerald-100/90'
  }
}

function buildBattleNotification(
  fenBeforeMove: string,
  move: string,
  status: PositionStatus,
): Omit<BattleNotification, 'id'> | null {
  const movingPiece = pieceAtSquare(fenBeforeMove, move.slice(0, 2))
  const capturedPiece = pieceAtSquare(fenBeforeMove, move.slice(2, 4))
  const captureDetail =
    movingPiece && capturedPiece
      ? `${pieceSideLabel(movingPiece)}${pieceDisplayName(movingPiece)}吃掉${pieceSideLabel(capturedPiece)}${pieceDisplayName(capturedPiece)}`
      : ''
  const captureTags = capturedPiece ? ['吃'] : []

  if (!status.hasLegalMoves) {
    return {
      title: '绝杀',
      detail: status.inCheck
        ? `${sideLabel(status.sideToMove)}无路可逃${captureDetail ? `，${captureDetail}` : ''}`
        : `${sideLabel(status.sideToMove)}已无合法着法${captureDetail ? `，${captureDetail}` : ''}`,
      tone: 'mate',
      tags: captureTags,
    }
  }

  if (status.inCheck) {
    return {
      title: '将军',
      detail: captureDetail ? `${captureDetail}，${sideLabel(status.sideToMove)}受将。` : `${sideLabel(status.sideToMove)}受将。`,
      tone: 'check',
      tags: captureTags,
    }
  }

  if (captureDetail) {
    return {
      title: '吃',
      detail: captureDetail,
      tone: 'capture',
      tags: [],
    }
  }

  return null
}

function updateBridgeStatusFromPosition(status: PositionStatus) {
  if (!status.hasLegalMoves) {
    bridgeStatus.value = `${sideLabel(status.sideToMove)}已无合法着法，对局结束。`
    return
  }

  if (status.inCheck) {
    bridgeStatus.value = `${sideLabel(status.sideToMove)}被将军。`
    return
  }

  refreshBridgeStatus()
}

function handleResolvedMove(fenBeforeMove: string, move: string) {
  if (!bridge.value) {
    return
  }

  const status = bridge.value.currentPositionStatus()
  const notification = buildBattleNotification(fenBeforeMove, move, status)
  if (notification) {
    showBattleNotification(notification)
  }
  updateBridgeStatusFromPosition(status)
}

function handleBoardFenChange(fen: string) {
  currentFen.value = fen
}

function handleHumanMoveApplied(payload: AppliedMovePayload) {
  currentFen.value = payload.fenAfter
  handleResolvedMove(payload.fenBefore, payload.move)
}

function ensureAiWorker() {
  if (aiWorker) {
    return aiWorker
  }

  const worker = new Worker(new URL('./workers/aiSearchWorker.ts', import.meta.url), { type: 'module' })
  worker.onmessage = (event: MessageEvent<AiSearchWorkerResponse>) => {
    const message = event.data
    if (message.type === 'progress') {
      if (pendingAiSearch && message.requestId === pendingAiSearch.requestId) {
        aiThinkingMove.value = message.progress.currentMove
      }
      return
    }

    if (!pendingAiSearch || message.requestId !== pendingAiSearch.requestId) {
      return
    }

    const pending = pendingAiSearch
    pendingAiSearch = null
    if (message.type === 'result') {
      pending.resolve(message.report)
      return
    }

    worker.terminate()
    aiWorker = null
    pending.reject(new Error(message.message))
  }
  worker.onerror = () => {
    const pending = pendingAiSearch
    pendingAiSearch = null
    worker.terminate()
    aiWorker = null
    pending?.reject(new Error('AI Worker 运行失败'))
  }

  aiWorker = worker
  return worker
}

function requestAiMoveReport(
  fen: string,
  maxDepth: number,
  timeBudgetMs: number,
): Promise<AiMoveReport | null> {
  if (pendingAiSearch) {
    disposeAiWorker(true)
  }

  const worker = ensureAiWorker()
  return new Promise<AiMoveReport | null>((resolve, reject) => {
    const requestId = ++nextAiSearchRequestId
    pendingAiSearch = {
      requestId,
      resolve,
      reject,
    }

    const message: AiSearchWorkerRequest = {
      type: 'search',
      requestId,
      fen,
      maxDepth,
      timeBudgetMs,
    }
    worker.postMessage(message)
  })
}

function cancelAiSearch() {
  aiThinking.value = false
  clearAiThinkingProgress()
  disposeAiWorker(true)
}

async function maybeRunAiTurn() {
  if (!bridge.value || !aiEnabled.value || aiThinking.value || activeSideToken.value !== aiSide.value) {
    return
  }

  const currentStatus = bridge.value.currentPositionStatus()
  if (!currentStatus.hasLegalMoves) {
    bridgeStatus.value = `${sideLabel(currentStatus.sideToMove)}已无合法着法，对局结束。`
    return
  }

  const fenBeforeAiMove = currentFen.value
  aiThinking.value = true
  clearAiThinkingProgress()
  refreshBridgeStatus()
  await nextTick()

  let aiReport: AiMoveReport | null = null
  try {
    aiReport = await requestAiMoveReport(fenBeforeAiMove, aiMaxDepth.value, aiTimeBudgetMs.value)
  } catch (error) {
    aiThinking.value = false
    clearAiThinkingProgress()
    bridgeStatus.value = 'AI 搜索失败，请稍后重试。'
    return
  }
  aiThinking.value = false
  clearAiThinkingProgress()

  if (!aiReport) {
    if (!bridge.value || !aiEnabled.value || currentFen.value !== fenBeforeAiMove) {
      refreshBridgeStatus()
      return
    }

    bridgeStatus.value = 'AI 当前无合法着法，对局结束。'
    return
  }

  if (!bridge.value || !aiEnabled.value || activeSideToken.value !== aiSide.value || currentFen.value !== fenBeforeAiMove) {
    refreshBridgeStatus()
    return
  }

  if (!bridge.value.applyMove(aiReport.move)) {
    bridgeStatus.value = 'AI 返回了非法着法，请重试。'
    return
  }

  lastAiReport.value = aiReport
  lastAiMoveLabel.value = describeAiMove(fenBeforeAiMove, aiReport.move)
  syncFromBridge()
  handleResolvedMove(fenBeforeAiMove, aiReport.move)
}

function resetBoard() {
  cancelAiSearch()
  clearAiThinkingProgress()
  clearAiInsight()
  clearBattleNotification()
  if (bridge.value) {
    bridge.value.reset()
  }
  syncFromBridge()
  resetKey.value += 1
  refreshBridgeStatus()
}

function undoBoard() {
  if (!bridge.value || !canUndo.value) {
    return
  }

  const stepsToUndo = aiEnabled.value ? 2 : 1
  for (let index = 0; index < stepsToUndo; index += 1) {
    if (!bridge.value.undoLastMove()) {
      break
    }
  }

  aiThinking.value = false
  clearAiThinkingProgress()
  clearAiInsight()
  clearBattleNotification()
  syncFromBridge()
  resetKey.value += 1
  bridgeStatus.value = aiEnabled.value ? '已悔一回合，回到你做决定之前。' : '已悔最近一步。'
}

function setAiEnabled(enabled: boolean) {
  if (!enabled && aiThinking.value) {
    cancelAiSearch()
  }
  aiEnabled.value = enabled
}

function setHumanSide(side: 'w' | 'b') {
  humanSide.value = side
  resetBoard()
}

function toggleAi() {
  setAiEnabled(!aiEnabled.value)
}

function setAiDifficulty(difficulty: AiDifficultyKey) {
  aiDifficulty.value = difficulty
}

onMounted(async () => {
  try {
    const wasmBridge = await createWasmBridge()
    bridge.value = wasmBridge
    syncFromBridge()
    refreshBridgeStatus()
  } catch (error) {
    bridgeStatus.value = '规则模块加载失败，请刷新页面后重试。'
  }
})

onBeforeUnmount(() => {
  disposeAiWorker(true)
  clearBattleNotification()
})

watch([currentFen, aiEnabled, bridge, humanSide], () => {
  if (!bridge.value) {
    return
  }

  const positionStatus = bridge.value.currentPositionStatus()

  if (aiEnabled.value && activeSideToken.value === aiSide.value && !aiThinking.value) {
    if (!positionStatus.hasLegalMoves) {
      bridgeStatus.value = `${sideLabel(positionStatus.sideToMove)}已无合法着法，对局结束。`
      return
    }
    void maybeRunAiTurn()
    return
  }

  if (!aiThinking.value) {
    updateBridgeStatusFromPosition(positionStatus)
  }
})

watchEffect(() => {
  window.__CHINESE_CHESS_TEST_API__ = {
    applyMove(move: string) {
      if (!bridge.value) {
        return false
      }

      const applied = bridge.value.applyMove(move)
      if (applied) {
        syncFromBridge()
      }
      return applied
    },
    undoLastMove() {
      if (!bridge.value) {
        return false
      }

      const undone = bridge.value.undoLastMove()
      if (undone) {
        clearAiInsight()
        syncFromBridge()
      }
      return undone
    },
    undoCount() {
      return undoCount.value
    },
    applyAiMove(maxDepth: number, timeBudgetMs?: number) {
      if (!bridge.value) {
        return null
      }

      const move = bridge.value.applyAiMove(maxDepth, timeBudgetMs)
      if (!move) {
        return null
      }

      lastAiMoveLabel.value = move
      syncFromBridge()
      refreshBridgeStatus()
      return move
    },
    applyAiMoveWithReport(maxDepth: number, timeBudgetMs: number) {
      if (!bridge.value) {
        return null
      }

      const report = bridge.value.applyAiMoveWithReport(maxDepth, timeBudgetMs)
      if (!report) {
        return null
      }

      lastAiReport.value = report
      lastAiMoveLabel.value = report.move
      syncFromBridge()
      refreshBridgeStatus()
      return report
    },
    currentFen() {
      return currentFen.value
    },
    legalMovesFrom(square: string) {
      return bridge.value?.legalMovesFrom(square) ?? []
    },
    setAiEnabled(enabled: boolean) {
      setAiEnabled(enabled)
    },
    setAiDifficulty(difficulty: AiDifficultyKey) {
      setAiDifficulty(difficulty)
    },
    setHumanSide(side: 'w' | 'b') {
      setHumanSide(side)
    },
    reset() {
      resetBoard()
    },
  }
})
</script>

<template>
  <div
    class="min-h-screen bg-[radial-gradient(circle_at_top_right,rgba(212,106,49,0.18),transparent_24%),radial-gradient(circle_at_left_bottom,rgba(135,38,28,0.18),transparent_26%),linear-gradient(180deg,#11171e,#0b0f13_44%,#07090c)] text-stone-100"
  >
    <div class="mx-auto grid w-full max-w-[1500px] gap-6 px-4 py-5 xl:grid-cols-[390px_minmax(0,1fr)]">
      <aside
        class="relative overflow-hidden rounded-[32px] border border-white/10 bg-white/[0.06] p-7 shadow-[0_24px_80px_rgba(10,8,6,0.34)] backdrop-blur-xl"
      >
        <div class="absolute inset-0 bg-[linear-gradient(145deg,rgba(255,255,255,0.08),transparent_34%,transparent_72%,rgba(255,255,255,0.03))]"></div>
        <div class="relative">
          <span
            class="block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.28em] text-stone-300/80"
          >
            Chinese Chess / Xiangqi
          </span>
          <h1
            class="mt-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[clamp(3rem,6vw,4.75rem)] leading-none text-stone-50"
          >
            楚河汉界
          </h1>
          <p class="mt-4 max-w-md leading-8 text-stone-300/80">
            一局可以直接开始的中国象棋。点击棋子、选择落点，就能在浏览器里完成整盘对弈。
          </p>

          <div
            class="mt-6 rounded-[26px] border border-white/10 bg-[radial-gradient(circle_at_top_left,rgba(212,106,49,0.2),transparent_52%),rgba(255,248,238,0.08)] px-5 py-5 backdrop-blur"
          >
            <span
              class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
            >
              游戏提示
            </span>
            <strong class="mt-2 block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-xl leading-8">
              {{ bridgeStatus }}
            </strong>
          </div>

            <div class="mt-4 grid gap-4 md:grid-cols-2 xl:grid-cols-2">
              <div class="rounded-[22px] border border-white/10 bg-white/[0.06] px-4 py-4">
              <span
                class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
              >
                当前走方
              </span>
              <strong class="mt-2 block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-2xl">
                {{ activeSide }}
              </strong>
              </div>
              <div class="rounded-[22px] border border-white/10 bg-white/[0.06] px-4 py-4">
                <span
                  class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
                >
                  对弈模式
                </span>
                <strong class="mt-2 block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-2xl">
                  {{ modeLabel }}
                </strong>
              </div>
            </div>

            <div class="mt-5 rounded-[24px] border border-white/10 bg-white/[0.05] px-4 py-4">
              <div class="flex items-center justify-between gap-3">
                <span
                  class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
                >
                  开局先手
                </span>
                <span class="font-mono text-xs text-stone-400/70">
                  {{ humanSide === 'w' ? '我执红' : 'AI 执红' }}
                </span>
              </div>
              <div class="mt-3 flex flex-wrap gap-2">
                <button
                  type="button"
                  class="rounded-full border px-4 py-2 text-sm transition"
                  :class="
                    humanSide === 'w'
                      ? 'border-amber-300/35 bg-amber-400/15 text-amber-50'
                      : 'border-white/10 bg-black/20 text-stone-200/85 hover:border-amber-200/35 hover:bg-white/[0.06]'
                  "
                  @click="setHumanSide('w')"
                >
                  我先（执红）
                </button>
                <button
                  type="button"
                  class="rounded-full border px-4 py-2 text-sm transition"
                  :class="
                    humanSide === 'b'
                      ? 'border-amber-300/35 bg-amber-400/15 text-amber-50'
                      : 'border-white/10 bg-black/20 text-stone-200/85 hover:border-amber-200/35 hover:bg-white/[0.06]'
                  "
                  @click="setHumanSide('b')"
                >
                  AI先（AI执红）
                </button>
              </div>
            </div>

            <div class="mt-5 rounded-[24px] border border-white/10 bg-[linear-gradient(180deg,rgba(255,255,255,0.06),rgba(255,255,255,0.03))] p-4">
              <div class="flex items-center justify-between gap-3">
                <span
                  class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
                >
                  局面控制
                </span>
                <span class="font-mono text-xs text-stone-400/70">
                  {{ aiEnabled ? `${selectedAiPreset.label} · 深度 ${aiMaxDepth} / ${aiTimeBudgetLabel}` : 'AI 已关闭' }}
                </span>
              </div>
              <div class="mt-3 rounded-[18px] border border-white/8 bg-black/20 px-4 py-3">
                <div class="flex flex-wrap items-center justify-between gap-2 text-xs">
                  <span class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-stone-200/85">AI 难度</span>
                  <span class="font-mono tracking-wide text-stone-400/75">
                    {{ `${selectedAiPreset.label} · 深度 ${selectedAiPreset.maxDepth} / ${formatAiTimeBudget(selectedAiPreset.timeBudgetMs)}` }}
                  </span>
                </div>
                <div class="mt-3 grid gap-2 sm:grid-cols-2">
                  <button
                    v-for="preset in AI_DIFFICULTY_PRESETS"
                    :key="preset.key"
                    type="button"
                    class="rounded-[18px] border px-4 py-3 text-left transition"
                    :class="
                      selectedAiPreset.key === preset.key
                        ? 'border-amber-300/35 bg-amber-400/12 text-amber-50 shadow-[0_10px_28px_rgba(212,106,49,0.18)]'
                        : 'border-white/10 bg-white/[0.03] text-stone-200/85 hover:border-amber-200/35 hover:bg-white/[0.06]'
                    "
                    @click="setAiDifficulty(preset.key)"
                  >
                    <span class="block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-base">{{ preset.label }}</span>
                    <span class="mt-1 block font-mono text-[11px] text-stone-300/75">
                      {{ `深度 ${preset.maxDepth} / ${formatAiTimeBudget(preset.timeBudgetMs)}` }}
                    </span>
                    <span class="mt-2 block text-xs leading-5 text-stone-300/70">
                      {{ preset.summary }}
                    </span>
                  </button>
                </div>
              </div>
              <div class="mt-3 grid gap-3 sm:grid-cols-3">
                <button
                  type="button"
                  class="rounded-[20px] bg-[linear-gradient(135deg,#8e2e1c,#d46a31)] px-5 py-4 text-left font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-base text-amber-50 shadow-[0_12px_30px_rgba(212,106,49,0.32)] transition hover:scale-[1.02]"
                  @click="resetBoard"
                >
                  <span class="block text-lg">重置棋盘</span>
                  <span class="mt-1 block text-xs text-amber-100/80">回到标准开局</span>
                </button>
                <button
                  type="button"
                  class="rounded-[20px] border px-5 py-4 text-left transition"
                  :class="
                    canUndo
                      ? 'border-emerald-200/25 bg-[linear-gradient(135deg,rgba(62,106,90,0.42),rgba(23,33,31,0.72))] text-stone-50 shadow-[inset_0_1px_0_rgba(255,255,255,0.08),0_10px_30px_rgba(7,12,10,0.24)] hover:border-emerald-200/40 hover:bg-[linear-gradient(135deg,rgba(76,126,107,0.5),rgba(26,37,34,0.8))]'
                      : 'border-white/10 bg-black/20 text-stone-500'
                  "
                  :disabled="!canUndo"
                  @click="undoBoard"
                >
                  <span class="block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-lg">{{ undoButtonLabel }}</span>
                  <span class="mt-1 block text-xs" :class="canUndo ? 'text-emerald-100/75' : 'text-stone-500'">
                    {{ undoHint }}
                  </span>
                </button>
                <button
                  type="button"
                  class="rounded-[20px] border border-white/15 bg-white/[0.06] px-5 py-4 text-left font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-base text-stone-100 transition hover:border-amber-200/35 hover:bg-white/[0.1]"
                  @click="toggleAi"
                >
                  <span class="block text-lg">{{ aiEnabled ? '关闭 AI 对手' : '开启 AI 对手' }}</span>
                  <span class="mt-1 block text-xs text-stone-300/70">
                    {{ aiEnabled ? '切换到双人对弈' : '恢复与 AI 对弈' }}
                  </span>
                </button>
              </div>
              <div class="mt-3 rounded-[18px] border border-white/8 bg-black/20 px-4 py-3">
                <div class="flex flex-wrap items-center justify-between gap-2 text-xs">
                  <span class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-stone-200/85">悔棋说明</span>
                  <span class="font-mono tracking-wide text-stone-400/75">{{ undoAvailabilityLabel }}</span>
                </div>
                <p class="mt-2 leading-6 text-stone-300/75">
                  {{ undoHint }}
                </p>
              </div>
            </div>

          <section class="mt-8">
            <div
              class="mb-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.26em] text-stone-300/70"
            >
              AI 洞察
            </div>
            <div class="grid gap-3">
              <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4 leading-7 text-stone-200/90">
                <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">AI 上一步</div>
                <div class="mt-2 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-lg text-stone-50">
                  {{ lastAiMoveLabel || '等待 AI 落子' }}
                </div>
              </div>
              <div class="grid gap-3 sm:grid-cols-2">
                <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4">
                  <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">AI 评估</div>
                  <div class="mt-2 font-mono text-2xl text-amber-100">{{ aiEvalLabel }}</div>
                </div>
                <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4">
                  <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">完成深度</div>
                  <div class="mt-2 font-mono text-2xl text-amber-100">
                    {{ lastAiReport?.completedDepth ?? '—' }}
                  </div>
                </div>
                <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4">
                  <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">搜索节点</div>
                  <div class="mt-2 font-mono text-lg text-stone-50">
                    {{ lastAiReport?.visitedNodes?.toLocaleString() ?? '—' }}
                  </div>
                </div>
                <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4">
                  <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">耗时</div>
                  <div class="mt-2 font-mono text-lg text-stone-50">
                    {{ lastAiReport ? `${lastAiReport.elapsedMs}ms` : '—' }}
                  </div>
                </div>
              </div>
              <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4 leading-7 text-stone-200/90">
                <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">主变化线</div>
                <div class="mt-2 break-words font-mono text-sm text-amber-100/90">
                  {{ aiPvLabel }}
                </div>
              </div>
              <div class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4 leading-7 text-stone-200/90">
                <div class="text-[11px] uppercase tracking-[0.24em] text-stone-400/70">开始之前</div>
                <ul class="mt-2 grid gap-2">
                  <li v-for="item in setupTips" :key="item">{{ item }}</li>
                </ul>
              </div>
            </div>
          </section>
        </div>
      </aside>

      <main
        class="relative overflow-hidden rounded-[32px] border border-white/10 bg-white/[0.05] p-6 shadow-[0_24px_80px_rgba(10,8,6,0.34)] backdrop-blur-xl"
      >
        <div class="absolute inset-0 bg-[linear-gradient(145deg,rgba(255,255,255,0.08),transparent_34%,transparent_72%,rgba(255,255,255,0.03))]"></div>
        <div class="relative">
          <header class="grid gap-6 xl:grid-cols-[minmax(0,1fr)_360px] xl:items-end">
            <div>
              <h2 class="mt-2 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[clamp(2rem,4vw,3rem)]">
                对弈棋盘
              </h2>
            </div>
            <p class="leading-8 text-stone-300/80">
              {{
                aiEnabled
                  ? '选择开局先手后开始对弈；AI 的路径、评估值和搜索摘要会实时显示在左侧。'
                  : '点击棋盘上的己方棋子后，再点击高亮落点完成双人对弈。'
              }}
            </p>
          </header>

            <div class="relative">
              <div
                v-if="battleNotification"
                :key="battleNotification.id"
                class="pointer-events-none absolute inset-x-0 top-8 z-30 flex justify-center px-4"
              >
                <div
                  class="w-full max-w-[560px] rounded-[28px] px-5 py-4 backdrop-blur-xl"
                  :class="battleNotificationFrameClass(battleNotification.tone)"
                >
                  <div class="min-w-0">
                    <div class="flex flex-wrap items-center gap-2">
                      <span
                        class="rounded-full px-3 py-1 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-sm tracking-[0.18em]"
                        :class="battleNotificationTitleClass(battleNotification.tone)"
                      >
                        {{ battleNotification.title }}
                      </span>
                      <span
                        v-for="tag in battleNotification.tags"
                        :key="tag"
                        class="rounded-full px-2.5 py-1 text-xs tracking-[0.16em]"
                        :class="battleNotificationTagClass(battleNotification.tone)"
                      >
                        {{ tag }}
                      </span>
                    </div>
                    <p class="mt-3 break-words font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-lg leading-8 text-stone-50">
                      {{ battleNotification.detail }}
                    </p>
                  </div>
                </div>
              </div>

              <PhaserBoard
                :bridge="bridge"
                :fen="currentFen"
                :bottom-side="humanSide"
                :highlight-move="aiThinking ? undefined : lastAiReport?.move || undefined"
                :thinking-move="aiThinkingMove || undefined"
                :interaction-locked="interactionLocked"
                :reset-key="resetKey"
                @fen-change="handleBoardFenChange"
                @move-applied="handleHumanMoveApplied"
              />
            </div>
        </div>
      </main>
    </div>
  </div>
</template>
