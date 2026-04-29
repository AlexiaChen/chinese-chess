<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch, watchEffect } from 'vue'

import type { BrowserBridge } from './bridge/wasmBridge'
import { createWasmBridge } from './bridge/wasmBridge'
import PhaserBoard from './components/PhaserBoard.vue'
import { INITIAL_FEN, sideLabel } from './game/fen'

const currentFen = ref(INITIAL_FEN)
const resetKey = ref(0)
const bridge = ref<BrowserBridge | null>(null)
const bridgeStatus = ref('棋盘正在加载，请稍候…')
const moveHistory = ref<string[]>([])
const aiEnabled = ref(true)
const aiThinking = ref(false)

const AI_SEARCH_DEPTH = 2
const AI_SIDE = 'b'

declare global {
  interface Window {
    __CHINESE_CHESS_TEST_API__?: {
      applyMove: (move: string) => boolean
      applyAiMove: (depth: number) => string | null
      currentFen: () => string
      legalMovesFrom: (square: string) => string[]
      setAiEnabled: (enabled: boolean) => void
      reset: () => void
    }
  }
}

const activeSideToken = computed(() => currentFen.value.split(' ')[1] ?? 'w')
const activeSide = computed(() => sideLabel(activeSideToken.value))
const modeLabel = computed(() => (aiEnabled.value ? '人机对弈（AI 执黑）' : '双人对弈'))
const setupTips: string[] = [
  '红方先行，先点击己方棋子，再选择落点。',
  '棋盘下方会显示当前可走的棋子和合法着法。',
  '默认是人机对弈，AI 执黑；也可以随时关闭 AI 切回双人对弈。',
]

function refreshBridgeStatus() {
  if (!bridge.value) {
    bridgeStatus.value = '棋盘正在加载，请稍候…'
    return
  }

  if (aiThinking.value) {
    bridgeStatus.value = `AI 正在思考（深度 ${AI_SEARCH_DEPTH}）…`
    return
  }

  if (!aiEnabled.value) {
    bridgeStatus.value = '棋盘已准备好，可双人对弈。'
    return
  }

  if (activeSideToken.value === AI_SIDE) {
    bridgeStatus.value = '轮到 AI 行棋。'
    return
  }

  bridgeStatus.value = '棋盘已准备好，你执红先行。'
}

async function maybeRunAiTurn() {
  if (!bridge.value || !aiEnabled.value || aiThinking.value || activeSideToken.value !== AI_SIDE) {
    return
  }

  aiThinking.value = true
  refreshBridgeStatus()
  await nextTick()

  const aiMove = bridge.value.applyAiMove(AI_SEARCH_DEPTH)
  aiThinking.value = false

  if (!aiMove) {
    bridgeStatus.value = 'AI 当前无合法着法，对局结束。'
    return
  }

  currentFen.value = bridge.value.currentFen()
  moveHistory.value = [...moveHistory.value, aiMove].slice(-8)
  bridgeStatus.value = `AI 已落子 ${aiMove}，轮到你继续。`
}

function resetBoard() {
  aiThinking.value = false
  if (bridge.value) {
    bridge.value.reset()
    currentFen.value = bridge.value.currentFen()
  } else {
    currentFen.value = INITIAL_FEN
  }
  moveHistory.value = []
  resetKey.value += 1
  refreshBridgeStatus()
}

function setAiEnabled(enabled: boolean) {
  aiEnabled.value = enabled
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

watch([currentFen, aiEnabled, bridge], () => {
  if (!bridge.value) {
    return
  }

  if (aiEnabled.value && activeSideToken.value === AI_SIDE && !aiThinking.value) {
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
        moveHistory.value = [...moveHistory.value, move].slice(-8)
      }
      return applied
    },
    applyAiMove(depth: number) {
      if (!bridge.value) {
        return null
      }

      const move = bridge.value.applyAiMove(depth)
      if (!move) {
        return null
      }

      currentFen.value = bridge.value.currentFen()
      moveHistory.value = [...moveHistory.value, move].slice(-8)
      refreshBridgeStatus()
      return move
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
                {{ aiEnabled ? `AI 深度 ${AI_SEARCH_DEPTH}` : 'AI 已关闭' }}
              </span>
            </div>

          <section class="mt-8">
            <div
              class="mb-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.26em] text-stone-300/70"
            >
              开始之前
            </div>
            <ul class="grid gap-3">
              <li
                v-for="item in setupTips"
                :key="item"
                class="relative rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4 pl-12 leading-7 text-stone-200/90"
              >
                <span
                  class="absolute left-4 top-[18px] h-4 w-4 rounded-full bg-[linear-gradient(180deg,#e0a463,#b74b21)] shadow-[0_0_0_6px_rgba(212,106,49,0.12)]"
                ></span>
                {{ item }}
              </li>
            </ul>
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
                  ? '你执红先行；每次你落子后，AI 会自动以黑方应手。'
                  : '点击棋盘上的己方棋子后，再点击高亮落点，或者使用棋盘下方的操作按钮完成走子。'
              }}
            </p>
          </header>

            <PhaserBoard
              :bridge="bridge"
              :fen="currentFen"
              :interaction-locked="aiThinking"
              :reset-key="resetKey"
              @fen-change="currentFen = $event"
              @move-applied="moveHistory = [...moveHistory, $event].slice(-8)"
            />

          <section
            class="mt-5 rounded-[24px] border border-white/10 bg-black/20 px-4 py-4"
          >
            <div class="flex items-center justify-between gap-3">
              <span
                class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
              >
                最近着法
              </span>
              <span class="text-sm text-stone-300/60">
                {{ moveHistory.length === 0 ? '等待落子' : `共 ${moveHistory.length} 步` }}
              </span>
            </div>

            <div
              class="mt-3 grid gap-2 md:grid-cols-2 xl:grid-cols-4"
            >
              <div
                v-for="move in moveHistory"
                :key="move"
                class="rounded-2xl border border-white/8 bg-white/[0.04] px-3 py-2 text-center font-mono text-sm tracking-wide text-amber-100/80"
              >
                {{ move }}
              </div>
              <div
                v-if="moveHistory.length === 0"
                class="rounded-2xl border border-dashed border-white/10 px-3 py-3 text-center text-sm text-stone-400/70 md:col-span-2 xl:col-span-4"
              >
                先点击己方棋子，再选择一个合法落点开始对弈。
              </div>
            </div>
          </section>
        </div>
      </main>
    </div>
  </div>
</template>
