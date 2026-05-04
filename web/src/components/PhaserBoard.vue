<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'

import type { BrowserBridge } from '../bridge/wasmBridge'
import {
  BOARD_METRICS,
  orientForBottomSide,
  pointForBottomSide,
  type BoardBottomSide,
} from '../game/boardMetrics'
import { createBoardGame, type BoardGameHandle } from '../game/boardScene'
import { parseFen, pieceDisplayName, toUciSquare } from '../game/fen'

type MoveAppliedPayload = {
  move: string
  fenBefore: string
  fenAfter: string
}

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
  interactionLocked: {
    type: Boolean,
    default: false,
  },
  bottomSide: {
    type: String as () => BoardBottomSide,
    default: 'w',
  },
  highlightMove: {
    type: String,
    default: null,
  },
  thinkingMove: {
    type: String,
    default: null,
  },
})

const emit = defineEmits<{
  (event: 'fen-change', fen: string): void
  (event: 'move-applied', payload: MoveAppliedPayload): void
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
  displayFile: number
  displayRank: number
  isSelected: boolean
  isActiveSide: boolean
  label: string
}

type MoveOverlay = {
  move: string
  target: string
  file: number
  rank: number
  displayFile: number
  displayRank: number
}

type HighlightOverlay = {
  move: string
  fromX: number
  fromY: number
  toX: number
  toY: number
  from: string
  to: string
}

type ThinkingOverlay = {
  move: string
  square: string
  displayFile: number
  displayRank: number
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
    const oriented = orientForBottomSide(file, rank, props.bottomSide)
    const isRedPiece = piece === piece.toUpperCase()
    const isActiveSide = position.sideToMove === 'w' ? isRedPiece : !isRedPiece

    return [
      {
        key: `${square}-${piece}`,
        piece,
        square,
        file,
        rank,
        displayFile: oriented.file,
        displayRank: oriented.rank,
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
      displayFile: orientForBottomSide(
        target.charCodeAt(0) - 'a'.charCodeAt(0),
        9 - Number(target[1]),
        props.bottomSide,
      ).file,
      displayRank: orientForBottomSide(
        target.charCodeAt(0) - 'a'.charCodeAt(0),
        9 - Number(target[1]),
        props.bottomSide,
      ).rank,
    }
  }),
)

const highlightOverlay = computed<HighlightOverlay | null>(() => {
  if (!props.highlightMove) {
    return null
  }

  const from = props.highlightMove.slice(0, 2)
  const to = props.highlightMove.slice(2, 4)
  const fromPoint = pointForBottomSide(
    from.charCodeAt(0) - 'a'.charCodeAt(0),
    9 - Number(from[1]),
    props.bottomSide,
  )
  const toPoint = pointForBottomSide(
    to.charCodeAt(0) - 'a'.charCodeAt(0),
    9 - Number(to[1]),
    props.bottomSide,
  )

  return {
    move: props.highlightMove,
    fromX: fromPoint.x,
    fromY: fromPoint.y,
    toX: toPoint.x,
    toY: toPoint.y,
    from,
    to,
  }
})

const thinkingOverlay = computed<ThinkingOverlay | null>(() => {
  if (!props.thinkingMove) {
    return null
  }

  const square = props.thinkingMove.slice(0, 2)
  const file = square.charCodeAt(0) - 'a'.charCodeAt(0)
  const rank = 9 - Number(square[1])
  const oriented = orientForBottomSide(file, rank, props.bottomSide)

  return {
    move: props.thinkingMove,
    square,
    displayFile: oriented.file,
    displayRank: oriented.rank,
  }
})

function percentStyle(file: number, rank: number, widthPercent: string, heightPercent: string) {
  const point = pointForBottomSide(file, rank, 'w')
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

  if (props.interactionLocked) {
    interactionHint.value = 'AI 正在思考，请稍候。'
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

  if (props.interactionLocked) {
    interactionHint.value = 'AI 正在思考，请稍候。'
    return
  }

  const applied = props.bridge.applyMove(move)
  if (!applied) {
    interactionHint.value = `落子 ${move} 失败`
    return
  }

  const fenBefore = props.fen
  const nextFen = props.bridge.currentFen()
  emit('fen-change', nextFen)
  emit('move-applied', {
    move,
    fenBefore,
    fenAfter: nextFen,
  })
  clearSelection(`已落子 ${move}`)
}

onMounted(() => {
  if (!container.value) {
    return
  }

  boardGame = createBoardGame(container.value, {
    bridge: props.bridge,
    fen: props.fen,
    bottomSide: props.bottomSide,
    onFenChange: (fen) => emit('fen-change', fen),
    onMoveApplied: (move) =>
      emit('move-applied', {
        move,
        fenBefore: props.fen,
        fenAfter: props.bridge?.currentFen() ?? props.fen,
      }),
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

watch(
  () => props.bottomSide,
  (bottomSide) => {
    boardGame?.setBottomSide(bottomSide)
    clearSelection(`当前轮到${sideToMove.value === 'w' ? '红方' : '黑方'}行棋`)
  },
)

watch(
  () => props.interactionLocked,
  (locked) => {
    if (locked) {
      clearSelection('AI 正在思考，请稍候。')
      return
    }

    clearSelection(`当前轮到${sideToMove.value === 'w' ? '红方' : '黑方'}行棋`)
  },
)
</script>

<template>
  <section
    class="mt-6 rounded-[28px] border border-white/8 bg-[linear-gradient(180deg,rgba(255,255,255,0.04),transparent),rgba(7,9,12,0.5)] p-4"
  >
    <div class="relative mx-auto w-full max-w-[860px] overflow-hidden rounded-[24px]">
      <div ref="container" class="min-h-[480px] w-full overflow-hidden rounded-[24px]"></div>

      <svg
        v-if="highlightOverlay"
        class="pointer-events-none absolute inset-0 z-[5] h-full w-full"
        :viewBox="`0 0 ${BOARD_METRICS.width} ${BOARD_METRICS.height}`"
      >
        <line
          :x1="highlightOverlay.fromX"
          :y1="highlightOverlay.fromY"
          :x2="highlightOverlay.toX"
          :y2="highlightOverlay.toY"
          stroke="rgba(245, 158, 11, 0.95)"
          stroke-width="10"
          stroke-linecap="round"
          stroke-dasharray="18 14"
        />
        <circle
          :cx="highlightOverlay.fromX"
          :cy="highlightOverlay.fromY"
          r="24"
          fill="rgba(245, 158, 11, 0.18)"
          stroke="rgba(252, 211, 77, 0.95)"
          stroke-width="4"
        />
        <circle
          :cx="highlightOverlay.toX"
          :cy="highlightOverlay.toY"
          r="24"
          fill="rgba(249, 115, 22, 0.28)"
          stroke="rgba(254, 215, 170, 0.95)"
          stroke-width="4"
        />
      </svg>

      <div class="absolute inset-0 z-10">
        <button
          v-for="overlay in pieceOverlays"
          :key="overlay.key"
          :aria-label="overlay.label"
          :data-square="overlay.square"
          :disabled="props.interactionLocked"
          class="absolute -translate-x-1/2 -translate-y-1/2 rounded-full border transition duration-150"
          :class="
            overlay.isSelected
              ? 'border-amber-300/90 bg-amber-200/15 shadow-[0_0_0_6px_rgba(245,158,11,0.18)]'
              : overlay.isActiveSide
                ? 'border-white/0 bg-transparent hover:border-amber-200/45 hover:bg-amber-100/5'
                : 'border-white/0 bg-transparent'
          "
          :style="percentStyle(overlay.displayFile, overlay.displayRank, '8.8%', '7.2%')"
          @click="handlePieceClick(overlay)"
        >
          <span class="sr-only">{{ overlay.label }}</span>
        </button>

        <button
          v-for="overlay in moveOverlays"
          :key="overlay.move"
          :aria-label="`落子 ${overlay.move}`"
          :data-move="overlay.move"
          :disabled="props.interactionLocked"
          class="absolute -translate-x-1/2 -translate-y-1/2 rounded-full border border-amber-100/80 bg-orange-500/85 text-[0px] shadow-[0_0_0_5px_rgba(249,115,22,0.18)] transition hover:scale-110"
          :style="percentStyle(overlay.displayFile, overlay.displayRank, '3.8%', '3.4%')"
          @click="handleMoveClick(overlay.move)"
        >
          落子 {{ overlay.target }}
        </button>

        <div
          v-if="thinkingOverlay"
          :aria-label="`AI 正在分析 ${thinkingOverlay.square}`"
          :data-thinking-move="thinkingOverlay.move"
          :data-thinking-square="thinkingOverlay.square"
          class="pointer-events-none absolute z-[12] -translate-x-1/2 -translate-y-1/2 rounded-lg border-4 border-emerald-300/95 bg-emerald-300/10 shadow-[0_0_0_6px_rgba(16,185,129,0.22),0_0_28px_rgba(110,231,183,0.35)] animate-pulse"
          :style="percentStyle(thinkingOverlay.displayFile, thinkingOverlay.displayRank, '8.8%', '7.2%')"
        ></div>
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

  </section>
</template>
