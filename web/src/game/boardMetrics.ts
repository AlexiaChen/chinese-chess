export const BOARD_METRICS = {
  width: 860,
  height: 980,
  paddingX: 96,
  paddingTop: 121,
  cellX: 82,
  cellY: 82,
  riverGap: 0,
  pieceRadius: 28,
} as const

export type BoardBottomSide = 'w' | 'b'

const LAST_FILE = 8
const LAST_RANK = 9

export function pointFor(file: number, rank: number): { x: number; y: number } {
  return {
    x: BOARD_METRICS.paddingX + file * BOARD_METRICS.cellX,
    y:
      BOARD_METRICS.paddingTop +
      rank * BOARD_METRICS.cellY +
      (rank >= 5 ? BOARD_METRICS.riverGap : 0),
  }
}

export function orientForBottomSide(
  file: number,
  rank: number,
  bottomSide: BoardBottomSide,
): { file: number; rank: number } {
  if (bottomSide === 'w') {
    return { file, rank }
  }

  return {
    file: LAST_FILE - file,
    rank: LAST_RANK - rank,
  }
}

export function pointForBottomSide(
  file: number,
  rank: number,
  bottomSide: BoardBottomSide,
): { x: number; y: number } {
  const oriented = orientForBottomSide(file, rank, bottomSide)
  return pointFor(oriented.file, oriented.rank)
}
