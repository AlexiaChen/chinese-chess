<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'

import type { BrowserBridge } from '../bridge/wasmBridge'
import { BOARD_METRICS, pointFor } from '../game/boardMetrics'
import { createBoardGame, type BoardGameHandle } from '../game/boardScene'
import { parseFen, pieceDisplayName, toUciSquare } from '../game/fen'

const props = defineProps({
  bridge: {
    type: Object as () => BrowserBridge | null,
    default: null,
  },
  fen: {
    type: String,
    required: true,
  },
  resetKey: {
    type: Number,
    default: 0,
  },
})

const emit = defineEmits<{
  (event: 'fen-change', fen: string): void
  (event: 'move-applied', move: string): void
}>()

const container = ref<HTMLElement | null>(null)
const selectedSquare = ref<string | null>(null)
const legalMoves = ref<string[]>([])
const interactionHint = ref('请选择当前走方的棋子')
let boardGame: BoardGameHandle | null = null

type PieceOverlay = {
  key: string
  piece: string
  square: string
  file: number
  rank: number
  isSelected: boolean
  isActiveSide: boolean
  label: string
}

type MoveOverlay = {
  move: string
  target: string
  file: number
  rank: number
}

const sideToMove = computed(() => parseFen(props.fen).sideToMove)

const pieceOverlays = computed<PieceOverlay[]>(() => {
  const position = parseFen(props.fen)
  return position.board.flatMap((piece, index) => {
    if (!piece) {
      return []
    }

    const file = index % 9
    const rank = Math.floor(index / 9)
    const square = toUciSquare(file, rank)
    const isRedPiece = piece === piece.toUpperCase()
    const isActiveSide = position.sideToMove === 'w' ? isRedPiece : !isRedPiece

    return [
      {
        key: `${square}-${piece}`,
        piece,
        square,
        file,
        rank,
        isSelected: selectedSquare.value === square,
        isActiveSide,
        label: `${isRedPiece ? '红方' : '黑方'}${pieceDisplayName(piece)} ${square}`,
      },
    ]
  })
})

const moveOverlays = computed<MoveOverlay[]>(() =>
  legalMoves.value.map((move) => {
    const target = move.slice(2, 4)
    return {
      move,
      target,
      file: target.charCodeAt(0) - 'a'.charCodeAt(0),
      rank: 9 - Number(target[1]),
    }
  }),
)

function percentStyle(file: number, rank: number, widthPercent: string, heightPercent: string) {
  const point = pointFor(file, rank)
  return {
    left: `${(point.x / BOARD_METRICS.width) * 100}%`,
    top: `${(point.y / BOARD_METRICS.height) * 100}%`,
    width: widthPercent,
    height: heightPercent,
  }
}

function clearSelection(hint = '请选择当前走方的棋子') {
  selectedSquare.value = null
  legalMoves.value = []
  interactionHint.value = hint
}

function handlePieceClick(overlay: PieceOverlay) {
  if (!props.bridge) {
    interactionHint.value = '棋盘还在加载，请稍候。'
    return
  }

  if (!overlay.isActiveSide) {
    interactionHint.value = `当前轮到${sideToMove.value === 'w' ? '红方' : '黑方'}行棋`
    clearSelection(interactionHint.value)
    return
  }

  if (selectedSquare.value === overlay.square) {
    clearSelection()
    return
  }

  const moves = props.bridge.legalMovesFrom(overlay.square)
  selectedSquare.value = overlay.square
  legalMoves.value = moves
  interactionHint.value =
    moves.length > 0
      ? `已选中 ${overlay.square}，可走 ${moves.length} 着`
      : `已选中 ${overlay.square}，当前无合法着法`
}

function handleMoveClick(move: string) {
  if (!props.bridge) {
    interactionHint.value = '棋盘还在加载，请稍候。'
    return
  }

  const applied = props.bridge.applyMove(move)
  if (!applied) {
    interactionHint.value = `落子 ${move} 失败`
    return
  }

  const nextFen = props.bridge.currentFen()
  emit('fen-change', nextFen)
  emit('move-applied', move)
  clearSelection(`已落子 ${move}`)
}

onMounted(() => {
  if (!container.value) {
    return
  }

  boardGame = createBoardGame(container.value, {
    bridge: props.bridge,
    fen: props.fen,
    onFenChange: (fen) => emit('fen-change', fen),
    onMoveApplied: (move) => emit('move-applied', move),
  })
})

onBeforeUnmount(() => {
  boardGame?.destroy()
  boardGame = null
})

watch(
  () => props.fen,
  (fen) => {
    boardGame?.setFen(fen)
    if (!selectedSquare.value) {
      interactionHint.value = `当前轮到${sideToMove.value === 'w' ? '红方' : '黑方'}行棋`
    }
  },
)

watch(
  () => props.resetKey,
  () => {
    boardGame?.setFen(props.fen)
    clearSelection()
  },
)

watch(
  () => props.bridge,
  (bridge) => {
    boardGame?.setBridge(bridge)
    if (!bridge) {
      clearSelection('棋盘还在加载，请稍候。')
    } else {
      clearSelection(`当前轮到${sideToMove.value === 'w' ? '红方' : '黑方'}行棋`)
    }
  },
  { immediate: true },
)
</script>

<template>
  <section
    class="mt-6 rounded-[28px] border border-white/8 bg-[linear-gradient(180deg,rgba(255,255,255,0.04),transparent),rgba(7,9,12,0.5)] p-4"
  >
    <div class="relative mx-auto w-full max-w-[860px] overflow-hidden rounded-[24px]">
      <div ref="container" class="min-h-[480px] w-full overflow-hidden rounded-[24px]"></div>

      <div class="absolute inset-0 z-10">
        <button
          v-for="overlay in pieceOverlays"
          :key="overlay.key"
          :aria-label="overlay.label"
          :data-square="overlay.square"
          class="absolute -translate-x-1/2 -translate-y-1/2 rounded-full border transition duration-150"
          :class="
            overlay.isSelected
              ? 'border-amber-300/90 bg-amber-200/15 shadow-[0_0_0_6px_rgba(245,158,11,0.18)]'
              : overlay.isActiveSide
                ? 'border-white/0 bg-transparent hover:border-amber-200/45 hover:bg-amber-100/5'
                : 'border-white/0 bg-transparent'
          "
          :style="percentStyle(overlay.file, overlay.rank, '8.8%', '7.2%')"
          @click="handlePieceClick(overlay)"
        >
          <span class="sr-only">{{ overlay.label }}</span>
        </button>

        <button
          v-for="overlay in moveOverlays"
          :key="overlay.move"
          :aria-label="`落子 ${overlay.move}`"
          :data-move="overlay.move"
          class="absolute -translate-x-1/2 -translate-y-1/2 rounded-full border border-amber-100/80 bg-orange-500/85 text-[0px] shadow-[0_0_0_5px_rgba(249,115,22,0.18)] transition hover:scale-110"
          :style="percentStyle(overlay.file, overlay.rank, '3.8%', '3.4%')"
          @click="handleMoveClick(overlay.move)"
        >
          落子 {{ overlay.target }}
        </button>
      </div>
    </div>

    <div class="mt-4 flex flex-wrap items-center justify-between gap-3 rounded-2xl border border-white/8 bg-white/[0.04] px-4 py-3">
      <p class="text-sm leading-7 text-stone-200/85">
        {{ interactionHint }}
      </p>
      <div class="flex flex-wrap gap-2">
        <span
          v-if="selectedSquare"
          class="rounded-full border border-amber-300/25 bg-amber-400/10 px-3 py-1 font-mono text-xs tracking-wide text-amber-100/85"
        >
          已选 {{ selectedSquare }}
        </span>
        <span
          class="rounded-full border border-white/8 bg-black/20 px-3 py-1 font-mono text-xs tracking-wide text-stone-300/70"
        >
          合法着法 {{ legalMoves.length }}
        </span>
      </div>
    </div>

    <div class="mt-4 grid gap-4 lg:grid-cols-[minmax(0,1fr)_minmax(0,0.9fr)]">
      <section class="rounded-2xl border border-white/8 bg-white/[0.04] px-4 py-4">
        <div class="mb-3 flex items-center justify-between gap-3">
          <h3 class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-sm tracking-[0.18em] text-stone-200/85">
            当前可选棋子
          </h3>
          <span class="font-mono text-xs text-stone-400/70">
            {{ sideToMove === 'w' ? '红方' : '黑方' }}
          </span>
        </div>

        <div class="flex flex-wrap gap-2">
          <button
            v-for="overlay in pieceOverlays.filter((item) => item.isActiveSide)"
            :key="`action-${overlay.key}`"
            type="button"
            class="rounded-full border px-3 py-1.5 text-sm transition"
            :class="
              overlay.isSelected
                ? 'border-amber-300/40 bg-amber-400/15 text-amber-50'
                : 'border-white/10 bg-black/20 text-stone-200/85 hover:border-amber-200/35 hover:bg-white/[0.06]'
            "
            @click="handlePieceClick(overlay)"
          >
            {{ pieceDisplayName(overlay.piece) }} {{ overlay.square }}
          </button>
        </div>
      </section>

      <section class="rounded-2xl border border-white/8 bg-white/[0.04] px-4 py-4">
        <div class="mb-3 flex items-center justify-between gap-3">
          <h3 class="font-[KaiTi,_Kaiti_SC,_STKaiti,_serif] text-sm tracking-[0.18em] text-stone-200/85">
            当前合法着法
          </h3>
          <span class="font-mono text-xs text-stone-400/70">
            {{ selectedSquare ?? '未选子' }}
          </span>
        </div>

        <div v-if="moveOverlays.length > 0" class="flex flex-wrap gap-2">
          <button
            v-for="overlay in moveOverlays"
            :key="`list-${overlay.move}`"
            type="button"
            class="rounded-full border border-amber-300/30 bg-orange-500/15 px-3 py-1.5 font-mono text-sm text-amber-50 transition hover:bg-orange-500/25"
            @click="handleMoveClick(overlay.move)"
          >
            {{ overlay.move }}
          </button>
        </div>
        <p v-else class="text-sm leading-7 text-stone-300/70">
          先选择一个己方棋子，再从这里确认落点。
        </p>
      </section>
    </div>
  </section>
</template>
