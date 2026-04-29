#include "engine/search.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace chinese_chess::engine {

namespace {

constexpr int kMateScore = 1'000'000;
constexpr int kInfinityScore = 2'000'000;

int piece_value(PieceType type) {
    switch (type) {
    case PieceType::General:
        return 100'000;
    case PieceType::Advisor:
        return 110;
    case PieceType::Elephant:
        return 110;
    case PieceType::Horse:
        return 400;
    case PieceType::Rook:
        return 900;
    case PieceType::Cannon:
        return 450;
    case PieceType::Soldier:
        return 100;
    case PieceType::None:
        return 0;
    }

    return 0;
}

int positional_bonus(Piece piece, Position position) {
    if (piece.type != PieceType::Soldier) {
        return 0;
    }

    const int forward_progress = piece.side == Side::Red ? (9 - position.rank) : position.rank;
    const bool crossed_river = piece.side == Side::Red ? position.rank <= 4 : position.rank >= 5;
    return forward_progress * 6 + (crossed_river ? 18 : 0);
}

int evaluate_red_perspective(const GameState& state) {
    int score = 0;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position position {file, rank};
            const Piece piece = state.piece_at(position);
            if (piece.is_empty()) {
                continue;
            }

            const int value = piece_value(piece.type) + positional_bonus(piece, position);
            score += piece.side == Side::Red ? value : -value;
        }
    }

    return score;
}

int evaluate_for_side_to_move(const GameState& state) {
    const int red_score = evaluate_red_perspective(state);
    return state.side_to_move() == Side::Red ? red_score : -red_score;
}

std::vector<Move> generate_all_legal_moves(const GameState& state) {
    std::vector<Move> moves;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position from {file, rank};
            const Piece piece = state.piece_at(from);
            if (piece.is_empty() || piece.side != state.side_to_move()) {
                continue;
            }

            std::vector<Move> piece_moves = state.generate_legal_moves(from);
            moves.insert(moves.end(), piece_moves.begin(), piece_moves.end());
        }
    }

    return moves;
}

int move_order_score(const GameState& state, const Move& move) {
    const Piece moving_piece = state.piece_at(move.from);
    const Piece captured_piece = state.piece_at(move.to);
    return piece_value(captured_piece.type) * 16 - piece_value(moving_piece.type);
}

void order_moves(const GameState& state, std::vector<Move>& moves) {
    std::stable_sort(moves.begin(), moves.end(), [&state](const Move& lhs, const Move& rhs) {
        return move_order_score(state, lhs) > move_order_score(state, rhs);
    });
}

int negamax(const GameState& state, int depth, int alpha, int beta, int ply) {
    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        return -kMateScore + ply;
    }

    if (depth == 0) {
        return evaluate_for_side_to_move(state);
    }

    order_moves(state, moves);

    int best_score = -kInfinityScore;
    for (const Move& move : moves) {
        GameState next = state;
        if (!next.apply_move(move)) {
            continue;
        }

        const int score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }

    return best_score;
}

}  // namespace

std::optional<Move> try_find_best_move(const GameState& state, int depth) {
    if (depth <= 0) {
        throw std::invalid_argument("Depth must be positive");
    }

    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        return std::nullopt;
    }

    order_moves(state, moves);

    std::optional<Move> best_move;
    int best_score = -kInfinityScore;
    int alpha = -kInfinityScore;
    const int beta = kInfinityScore;

    for (const Move& move : moves) {
        GameState next = state;
        if (!next.apply_move(move)) {
            continue;
        }

        const int score = -negamax(next, depth - 1, -beta, -alpha, 1);
        if (!best_move.has_value() || score > best_score) {
            best_score = score;
            best_move = move;
        }
        alpha = std::max(alpha, score);
    }

    return best_move;
}

Move find_best_move(const GameState& state, int depth) {
    const std::optional<Move> best_move = try_find_best_move(state, depth);
    if (!best_move.has_value()) {
        throw std::runtime_error("No legal moves available");
    }

    return *best_move;
}

}  // namespace chinese_chess::engine
