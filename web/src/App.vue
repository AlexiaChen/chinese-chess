<script setup lang="ts">
import { computed, onMounted, ref, watchEffect } from 'vue'

import type { BrowserBridge } from './bridge/wasmBridge'
import { createWasmBridge } from './bridge/wasmBridge'
import PhaserBoard from './components/PhaserBoard.vue'
import { INITIAL_FEN, sideLabel } from './game/fen'

const currentFen = ref(INITIAL_FEN)
const resetKey = ref(0)
const bridge = ref<BrowserBridge | null>(null)
const bridgeStatus = ref('正在等待 WASM 棋规核心…')
const moveHistory = ref<string[]>([])

declare global {
  interface Window {
    __CHINESE_CHESS_TEST_API__?: {
      applyMove: (move: string) => boolean
      currentFen: () => string
      legalMovesFrom: (square: string) => string[]
      reset: () => void
    }
  }
}

const activeSide = computed(() => sideLabel(currentFen.value.split(' ')[1] ?? 'w'))

const milestones: string[] = [
  'Vue 3 + Vite + Phaser 前端骨架已建立',
  '棋盘画布已切到 Pikafish 兼容 FEN 渲染',
  'WASM 棋规核心已接入，浏览器内可直接验证合法着法',
  'DOM overlay 已补齐点击入口，便于浏览器自动化回归',
]

const tracks: string[] = [
  '真实 Pikafish 已作为 third_party 子模块接入',
  '原生 CLI 已能读取真实引擎 bestmove',
  'Web 端正在沿用同一条 UCI / FEN 边界与 WASM 规则核心',
]

const runtimeBadge = computed(() =>
  bridge.value ? 'WASM 规则核心在线 / DOM 落子可测' : '当前仅显示静态棋盘壳',
)

function resetBoard() {
  if (bridge.value) {
    bridge.value.reset()
    currentFen.value = bridge.value.currentFen()
  } else {
    currentFen.value = INITIAL_FEN
  }
  moveHistory.value = []
  resetKey.value += 1
}

onMounted(async () => {
  try {
    const wasmBridge = await createWasmBridge()
    bridge.value = wasmBridge
    currentFen.value = wasmBridge.currentFen()
    bridgeStatus.value = 'WASM 棋规核心已连接，可返回合法走法与新局面'
  } catch (error) {
    bridgeStatus.value = '未找到 WASM 产物，当前显示静态棋盘壳'
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
    currentFen() {
      return currentFen.value
    },
    legalMovesFrom(square: string) {
      return bridge.value?.legalMovesFrom(square) ?? []
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
            这不是临时占位页，而是已经接上 WASM 棋规与浏览器可测交互层的中国象棋前端。
          </p>

          <div
            class="mt-6 rounded-[26px] border border-white/10 bg-[radial-gradient(circle_at_top_left,rgba(212,106,49,0.2),transparent_52%),rgba(255,248,238,0.08)] px-5 py-5 backdrop-blur"
          >
            <span
              class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
            >
              当前状态
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
                引擎边界
              </span>
              <strong class="mt-2 block font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-2xl">
                UCI / FEN
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
            <span
              class="inline-flex items-center rounded-full border border-white/12 bg-white/[0.06] px-4 py-2 text-sm text-stone-300/80"
            >
              {{ runtimeBadge }}
            </span>
          </div>

          <section class="mt-8">
            <div
              class="mb-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.26em] text-stone-300/70"
            >
              里程碑
            </div>
            <ul class="grid gap-3">
              <li
                v-for="item in milestones"
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

          <section class="mt-8">
            <div
              class="mb-3 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.26em] text-stone-300/70"
            >
              技术走向
            </div>
            <ul class="grid gap-3">
              <li
                v-for="item in tracks"
                :key="item"
                class="rounded-[20px] border border-white/8 bg-white/[0.05] px-4 py-4 leading-7 text-stone-200/90"
              >
                {{ item }}
              </li>
            </ul>
          </section>

          <section class="mt-8 rounded-[24px] border border-white/10 bg-black/20 px-4 py-4">
            <span
              class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[11px] uppercase tracking-[0.24em] text-stone-300/70"
            >
              当前 FEN
            </span>
            <code class="mt-3 block break-all font-mono text-[13px] leading-7 text-amber-100/80">
              {{ currentFen }}
            </code>
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
              <span
                class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[12px] uppercase tracking-[0.28em] text-stone-300/80"
              >
                Phaser Stage
              </span>
              <h2 class="mt-2 font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-[clamp(2rem,4vw,3rem)]">
                棋盘场景已可视化
              </h2>
            </div>
            <p class="leading-8 text-stone-300/80">
              现在由 Phaser 负责棋盘视觉层，DOM overlay 负责可点击与自动化测试入口，两者共同复用
              WASM 棋规核心。
            </p>
          </header>

            <PhaserBoard
              :bridge="bridge"
              :fen="currentFen"
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
                连接 WASM 后，点击己方棋子可查看合法着法并完成走子。
              </div>
            </div>
          </section>
        </div>
      </main>
    </div>
  </div>
</template>
