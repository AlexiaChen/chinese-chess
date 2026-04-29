import Phaser from 'phaser'

import type { BrowserBridge } from '../bridge/wasmBridge'
import { BOARD_METRICS, pointFor } from './boardMetrics'
import { parseFen, PIECE_LABELS } from './fen'

const COLORS = {
  canvas: 0x10161c,
  board: 0xd7aa69,
  boardShadow: 0xb07a31,
  line: 0x6b3714,
  red: '#9d241a',
  black: '#291f1a',
}

class XiangqiBoardScene extends Phaser.Scene {
  currentFen: string
  bridge: BrowserBridge | null
  onFenChange: (fen: string) => void
  onMoveApplied: (move: string) => void
  pieceLayer: Phaser.GameObjects.Container | null

  constructor(
    initialFen: string,
    bridge: BrowserBridge | null,
    onFenChange: (fen: string) => void,
    onMoveApplied: (move: string) => void,
  ) {
    super('xiangqi-board')
    this.currentFen = initialFen
    this.bridge = bridge
    this.onFenChange = onFenChange
    this.onMoveApplied = onMoveApplied
    this.pieceLayer = null
  }

  setFen(fen: string): void {
    this.currentFen = fen
    if (this.pieceLayer) {
      this.renderPieces()
    }
  }

  setBridge(bridge: BrowserBridge | null): void {
    this.bridge = bridge
  }

  create(): void {
    this.cameras.main.setBackgroundColor(COLORS.canvas)

    const background = this.add.graphics()
    background.fillStyle(COLORS.board, 1)
    background.fillRoundedRect(22, 22, BOARD_METRICS.width - 44, BOARD_METRICS.height - 44, 36)
    background.lineStyle(2, COLORS.boardShadow, 0.4)
    background.strokeRoundedRect(22, 22, BOARD_METRICS.width - 44, BOARD_METRICS.height - 44, 36)

    this.drawBoardGrid()
    this.drawRiverLabel()
    this.pieceLayer = this.add.container(0, 0)
    this.renderPieces()
  }

  drawBoardGrid(): void {
    const graphics = this.add.graphics()
    graphics.lineStyle(2, COLORS.line, 0.92)

    for (let rank = 0; rank < 10; rank += 1) {
      const from = pointFor(0, rank)
      const to = pointFor(8, rank)
      graphics.beginPath()
      graphics.moveTo(from.x, from.y)
      graphics.lineTo(to.x, to.y)
      graphics.strokePath()
    }

    for (let file = 0; file < 9; file += 1) {
      const topStart = pointFor(file, 0)
      const topEnd = pointFor(file, 4)
      const bottomStart = pointFor(file, 5)
      const bottomEnd = pointFor(file, 9)

      graphics.beginPath()
      graphics.moveTo(topStart.x, topStart.y)
      graphics.lineTo(topEnd.x, topEnd.y)
      graphics.strokePath()

      if (file === 0 || file === 8) {
        graphics.beginPath()
        graphics.moveTo(topEnd.x, topEnd.y)
        graphics.lineTo(bottomStart.x, bottomStart.y)
        graphics.lineTo(bottomEnd.x, bottomEnd.y)
        graphics.strokePath()
      } else {
        graphics.beginPath()
        graphics.moveTo(bottomStart.x, bottomStart.y)
        graphics.lineTo(bottomEnd.x, bottomEnd.y)
        graphics.strokePath()
      }
    }

    this.drawPalace(graphics, 0)
    this.drawPalace(graphics, 7)
  }

  drawPalace(graphics: Phaser.GameObjects.Graphics, startRank: number): void {
    const topLeft = pointFor(3, startRank)
    const topRight = pointFor(5, startRank)
    const bottomLeft = pointFor(3, startRank + 2)
    const bottomRight = pointFor(5, startRank + 2)

    graphics.beginPath()
    graphics.moveTo(topLeft.x, topLeft.y)
    graphics.lineTo(bottomRight.x, bottomRight.y)
    graphics.moveTo(topRight.x, topRight.y)
    graphics.lineTo(bottomLeft.x, bottomLeft.y)
    graphics.strokePath()
  }

  drawRiverLabel(): void {
    const midY = (pointFor(0, 4).y + pointFor(0, 5).y) / 2

    this.add
      .text(BOARD_METRICS.width * 0.27, midY, '楚河', {
        fontFamily: '"Kaiti SC", "STKaiti", "KaiTi", serif',
        fontSize: '38px',
        color: '#6f4b2c',
      })
      .setOrigin(0.5)
      .setRotation(-0.02)

    this.add
      .text(BOARD_METRICS.width * 0.73, midY, '汉界', {
        fontFamily: '"Kaiti SC", "STKaiti", "KaiTi", serif',
        fontSize: '38px',
        color: '#6f4b2c',
      })
      .setOrigin(0.5)
      .setRotation(0.02)
  }

  renderPieces(): void {
    if (!this.pieceLayer) {
      return
    }

    this.pieceLayer.removeAll(true)

    const position = parseFen(this.currentFen)
    position.board.forEach((piece, index) => {
      if (!piece) {
        return
      }

      const file = index % 9
      const rank = Math.floor(index / 9)
      const point = pointFor(file, rank)
      const isRed = piece === piece.toUpperCase()
      const container = this.add.container(point.x, point.y)
      const shadow = this.add.circle(2, 4, BOARD_METRICS.pieceRadius, 0x000000, 0.14)
      const disc = this.add.circle(0, 0, BOARD_METRICS.pieceRadius, 0xf6e8c8, 1)
      disc.setStrokeStyle(2, 0x6e3c17, 0.34)

      const label = this.add
        .text(0, 0, PIECE_LABELS[piece], {
          fontFamily: '"Kaiti SC", "STKaiti", "KaiTi", serif',
          fontSize: '30px',
          color: isRed ? COLORS.red : COLORS.black,
        })
        .setOrigin(0.5)

      container.add([shadow, disc, label])
      container.setSize(BOARD_METRICS.pieceRadius * 2, BOARD_METRICS.pieceRadius * 2)

      container.setScale(0.88)
      this.tweens.add({
        targets: container,
        scale: 1,
        duration: 260,
        ease: 'Cubic.Out',
        delay: (file + rank) * 18,
      })

      this.pieceLayer?.add(container)
    })
  }
}

export interface BoardGameHandle {
  destroy: () => void
  setFen: (fen: string) => void
  setBridge: (bridge: BrowserBridge | null) => void
}

interface CreateBoardGameOptions {
  bridge: BrowserBridge | null
  fen: string
  onFenChange: (fen: string) => void
  onMoveApplied: (move: string) => void
}

export function createBoardGame(
  container: HTMLElement,
  options: CreateBoardGameOptions,
): BoardGameHandle {
  const scene = new XiangqiBoardScene(
    options.fen,
    options.bridge,
    options.onFenChange,
    options.onMoveApplied,
  )
  const game = new Phaser.Game({
    type: Phaser.AUTO,
    parent: container,
    width: BOARD_METRICS.width,
    height: BOARD_METRICS.height,
    backgroundColor: '#10161c',
    scene,
  })

  return {
    destroy() {
      game.destroy(true)
    },
    setFen(fen: string) {
      scene.setFen(fen)
    },
    setBridge(bridge: BrowserBridge | null) {
      scene.setBridge(bridge)
    },
  }
}
