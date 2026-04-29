export const BOARD_METRICS = {
  width: 860,
  height: 980,
  paddingX: 96,
  paddingTop: 98,
  cellX: 82,
  cellY: 68,
  riverGap: 86,
  pieceRadius: 28,
} as const

export function pointFor(file: number, rank: number): { x: number; y: number } {
  return {
    x: BOARD_METRICS.paddingX + file * BOARD_METRICS.cellX,
    y:
      BOARD_METRICS.paddingTop +
      rank * BOARD_METRICS.cellY +
      (rank >= 5 ? BOARD_METRICS.riverGap : 0),
  }
}
