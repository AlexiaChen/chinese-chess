#!/usr/bin/env python3

import sys


EXPECTED_FEN = "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1"


def main() -> int:
    last_fen = ""

    for raw_line in sys.stdin:
        line = raw_line.strip()
        if line == "uci":
            print("id name FakePikafish", flush=True)
            print("uciok", flush=True)
        elif line == "isready":
            print("readyok", flush=True)
        elif line.startswith("position fen "):
            last_fen = line[len("position fen "):]
        elif line.startswith("go depth "):
            if last_fen != EXPECTED_FEN:
                print("bestmove a0a0", flush=True)
            else:
                print("bestmove a3a4", flush=True)
        elif line == "quit":
            return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
