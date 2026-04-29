<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch, watchEffect } from 'vue'

import type { AiMoveReport, BrowserBridge } from './bridge/wasmBridge'
import { createWasmBridge } from './bridge/wasmBridge'
import PhaserBoard from './components/PhaserBoard.vue'
import { INITIAL_FEN, parseFen, pieceDisplayName, sideLabel } from './game/fen'

const currentFen = ref(INITIAL_FEN)
const resetKey = ref(0)
const bridge = ref<BrowserBridge | null>(null)
const bridgeStatus = ref('棋盘正在加载，请稍候…')
const aiEnabled = ref(true)
const aiThinking = ref(false)
const humanSide = ref<'w' | 'b'>('w')
const lastAiReport = ref<AiMoveReport | null>(null)
const lastAiMoveLabel = ref('')

const AI_MAX_DEPTH = 10
const AI_TIME_BUDGET_MS = 450

declare global {
  interface Window {
    __CHINESE_CHESS_TEST_API__?: {
      applyMove: (move: string) => boolean
      applyAiMove: (maxDepth: number, timeBudgetMs?: number) => string | null
      applyAiMoveWithReport: (maxDepth: number, timeBudgetMs: number) => AiMoveReport | null
      currentFen: () => string
      legalMovesFrom: (square: string) => string[]
      setAiEnabled: (enabled: boolean) => void
      setHumanSide: (side: 'w' | 'b') => void
      reset: () => void
    }
  }
}

const activeSideToken = computed(() => currentFen.value.split(' ')[1] ?? 'w')
const activeSide = computed(() => sideLabel(activeSideToken.value))
const aiSide = computed(() => (humanSide.value === 'w' ? 'b' : 'w'))
const humanTurn = computed(() => activeSideToken.value === humanSide.value)
const interactionLocked = computed(() => aiThinking.value || (aiEnabled.value && !humanTurn.value))
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
const setupTips: string[] = [
  '支持选择我先或 AI 先，开局会自动按选定方落子。',
  'AI 上一步会在棋盘上高亮路径，并在侧边栏展示评估、深度、节点数与主变化线。',
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
    bridgeStatus.value = `AI 正在思考（深度 ${AI_MAX_DEPTH}）…`
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

async function maybeRunAiTurn() {
  if (!bridge.value || !aiEnabled.value || aiThinking.value || activeSideToken.value !== aiSide.value) {
    return
  }

  const fenBeforeAiMove = currentFen.value
  aiThinking.value = true
  refreshBridgeStatus()
  await nextTick()

  const aiReport = bridge.value.applyAiMoveWithReport(AI_MAX_DEPTH, AI_TIME_BUDGET_MS)
  aiThinking.value = false

  if (!aiReport) {
    bridgeStatus.value = 'AI 当前无合法着法，对局结束。'
    return
  }

  lastAiReport.value = aiReport
  lastAiMoveLabel.value = describeAiMove(fenBeforeAiMove, aiReport.move)
  currentFen.value = bridge.value.currentFen()
  bridgeStatus.value = `AI 已落子 ${lastAiMoveLabel.value}`
}

function resetBoard() {
  aiThinking.value = false
  lastAiReport.value = null
  lastAiMoveLabel.value = ''
  if (bridge.value) {
    bridge.value.reset()
    currentFen.value = bridge.value.currentFen()
  } else {
    currentFen.value = INITIAL_FEN
  }
  resetKey.value += 1
  refreshBridgeStatus()
}

function setAiEnabled(enabled: boolean) {
  aiEnabled.value = enabled
}

function setHumanSide(side: 'w' | 'b') {
  humanSide.value = side
  resetBoard()
}

function toggleAi() {
  setAiEnabled(!aiEnabled.value)
}

onMounted(async () => {
  try {
    const wasmBridge = await createWasmBridge()
    bridge.value = wasmBridge
    currentFen.value = wasmBridge.currentFen()
    refreshBridgeStatus()
  } catch (error) {
    bridgeStatus.value = '规则模块加载失败，请刷新页面后重试。'
  }
})

watch([currentFen, aiEnabled, bridge, humanSide], () => {
  if (!bridge.value) {
    return
  }

  if (aiEnabled.value && activeSideToken.value === aiSide.value && !aiThinking.value) {
    void maybeRunAiTurn()
    return
  }

  if (!aiThinking.value) {
    refreshBridgeStatus()
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
        currentFen.value = bridge.value.currentFen()
      }
      return applied
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
      currentFen.value = bridge.value.currentFen()
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
      currentFen.value = bridge.value.currentFen()
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

            <div class="mt-5 flex flex-wrap items-center gap-3">
            <button
              type="button"
              class="rounded-full bg-[linear-gradient(135deg,#8e2e1c,#d46a31)] px-5 py-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-base text-amber-50 shadow-[0_12px_30px_rgba(212,106,49,0.32)] transition hover:scale-[1.02]"
              @click="resetBoard"
              >
                重置棋盘
              </button>
              <button
                type="button"
                class="rounded-full border border-white/15 bg-white/[0.06] px-5 py-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-base text-stone-100 transition hover:border-amber-200/35 hover:bg-white/[0.1]"
                @click="toggleAi"
              >
                {{ aiEnabled ? '关闭 AI 对手' : '开启 AI 对手' }}
              </button>
              <span
                class="rounded-full border border-white/10 bg-black/20 px-4 py-2 font-mono text-xs tracking-wide text-stone-300/75"
              >
                {{ aiEnabled ? `AI 深度 ${AI_MAX_DEPTH}` : 'AI 已关闭' }}
              </span>
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

            <PhaserBoard
              :bridge="bridge"
              :fen="currentFen"
              :highlight-move="lastAiReport?.move || undefined"
              :interaction-locked="interactionLocked"
              :reset-key="resetKey"
              @fen-change="currentFen = $event"
            />
        </div>
      </main>
    </div>
  </div>
</template>
