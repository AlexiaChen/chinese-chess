export const INITIAL_FEN =
  'rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1'

export const PIECE_LABELS: Record<string, string> = {
  r: '车',
  n: '马',
  b: '象',
  a: '士',
  k: '将',
  c: '炮',
  p: '卒',
  R: '车',
  N: '马',
  B: '相',
  A: '仕',
  K: '帅',
  C: '炮',
  P: '兵',
}

export interface ParsedFen {
  board: Array<string | null>
  sideToMove: string
}

export function pieceDisplayName(piece: string): string {
  return PIECE_LABELS[piece] ?? piece
}

export function toUciSquare(file: number, rank: number): string {
  return `${String.fromCharCode('a'.charCodeAt(0) + file)}${9 - rank}`
}

export function sideLabel(side: string): string {
  return side === 'b' ? '黑方' : '红方'
}

export function parseFen(fen: string): ParsedFen {
  const [boardPart, sideToMove = 'w'] = fen.split(' ')
  const ranks = boardPart.split('/')

  const board = ranks.flatMap((rankText) => {
    const rank: Array<string | null> = []
    for (const char of rankText) {
      if (/\d/.test(char)) {
        rank.push(...Array.from({ length: Number(char) }, () => null))
      } else {
        rank.push(char)
      }
    }
    return rank
  })

  if (board.length !== 90) {
    throw new Error(`Invalid Xiangqi FEN board length: ${board.length}`)
  }

  return {
    board,
    sideToMove,
  }
}
