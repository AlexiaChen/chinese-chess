#include "engine/search.h"
#include "engine/opening_book.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace chinese_chess::engine {

namespace {

constexpr int kMateScore = 1'000'000;
constexpr int kInfinityScore = 2'000'000;
constexpr int kHistoryTableSize = kBoardSquares * kBoardSquares;
constexpr int kMaxPly = 64;
constexpr std::size_t kTranspositionTableSize = 1U << 16;
constexpr int kInitialAspirationWindow = 48;

enum class BoundType {
    Exact,
    Lower,
    Upper,
};

struct TranspositionEntry {
    std::uint64_t key {};
    int depth {-1};
    int score {};
    BoundType bound {BoundType::Exact};
    Move best_move {};
    bool occupied {false};
};

struct SearchContext {
    SearchOptions options;
    bool has_deadline {false};
    bool timed_out {false};
    std::chrono::steady_clock::time_point deadline {};
    std::uint64_t visited_nodes {0};
    std::array<std::array<Move, 2>, kMaxPly> killer_moves {};
    std::array<std::array<bool, 2>, kMaxPly> killer_valid {};
    std::array<int, kHistoryTableSize> history_scores {};
    std::vector<TranspositionEntry> table {kTranspositionTableSize};
};

struct RootSearchResult {
    Move best_move;
    int best_score {};
};

int piece_value(PieceType type) {
    switch (type) {
    case PieceType::General:
        return 100'000;
    case PieceType::Advisor:
        return 120;
    case PieceType::Elephant:
        return 120;
    case PieceType::Horse:
        return 430;
    case PieceType::Rook:
        return 900;
    case PieceType::Cannon:
        return 460;
    case PieceType::Soldier:
        return 100;
    case PieceType::None:
        return 0;
    }

    return 0;
}

int positional_bonus(Piece piece, Position position) {
    const int center_distance = std::abs(position.file - 4) + std::abs(position.rank - 4);
    const int forward_progress = piece.side == Side::Red ? (9 - position.rank) : position.rank;
    const bool crossed_river = piece.side == Side::Red ? position.rank <= 4 : position.rank >= 5;

    switch (piece.type) {
    case PieceType::Soldier:
        return forward_progress * 10 + (crossed_river ? 24 : 0);
    case PieceType::Horse:
        return 24 - center_distance * 4;
    case PieceType::Cannon:
        return 18 - center_distance * 3;
    case PieceType::Rook:
        return 14 - center_distance * 2;
    case PieceType::Advisor:
    case PieceType::Elephant:
        return crossed_river ? -10 : 8;
    case PieceType::General:
        return position.file == 4 ? 12 : 0;
    case PieceType::None:
        return 0;
    }

    return 0;
}

std::uint64_t splitmix64(std::uint64_t seed) {
    seed += 0x9e3779b97f4a7c15ULL;
    seed = (seed ^ (seed >> 30)) * 0xbf58476d1ce4e5b9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94d049bb133111ebULL;
    return seed ^ (seed >> 31);
}

int piece_hash_index(Piece piece) {
    if (piece.is_empty()) {
        return -1;
    }

    const int side_offset = piece.side == Side::Red ? 0 : 7;
    return side_offset + static_cast<int>(piece.type) - 1;
}

std::uint64_t position_hash(Position position, Piece piece) {
    return splitmix64(
        static_cast<std::uint64_t>(position.rank * kBoardFiles + position.file) * 17ULL
        + static_cast<std::uint64_t>(piece_hash_index(piece) + 1));
}

std::uint64_t compute_hash(const GameState& state) {
    std::uint64_t hash = state.side_to_move() == Side::Red
        ? 0xf0f0f0f0f0f0f0f1ULL
        : 0x0f0f0f0f0f0f0f0eULL;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position position {file, rank};
            const Piece piece = state.piece_at(position);
            if (piece.is_empty()) {
                continue;
            }
            hash ^= position_hash(position, piece);
        }
    }

    return hash;
}

int move_index(const Move& move) {
    const int from_index = move.from.rank * kBoardFiles + move.from.file;
    const int to_index = move.to.rank * kBoardFiles + move.to.file;
    return from_index * kBoardSquares + to_index;
}

bool is_capture(const GameState& state, const Move& move) {
    return !state.piece_at(move.to).is_empty();
}

bool has_null_move_material(const GameState& state, Side side) {
    int heavy_piece_count = 0;
    int soldier_count = 0;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Piece piece = state.piece_at(Position {file, rank});
            if (piece.is_empty() || piece.side != side) {
                continue;
            }

            switch (piece.type) {
            case PieceType::Rook:
            case PieceType::Horse:
            case PieceType::Cannon:
                ++heavy_piece_count;
                break;
            case PieceType::Soldier:
                ++soldier_count;
                break;
            case PieceType::General:
            case PieceType::Advisor:
            case PieceType::Elephant:
            case PieceType::None:
                break;
            }
        }
    }

    return heavy_piece_count >= 2 || (heavy_piece_count >= 1 && soldier_count >= 3);
}

Side opposite(Side side) {
    return side == Side::Red ? Side::Black : Side::Red;
}

void check_timeout(SearchContext& context) {
    ++context.visited_nodes;
    if (!context.has_deadline) {
        return;
    }

    if ((context.visited_nodes & 63ULL) != 0ULL) {
        return;
    }

    if (std::chrono::steady_clock::now() >= context.deadline) {
        context.timed_out = true;
    }
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

    if (state.is_in_check(Side::Black)) {
        score += 60;
    }
    if (state.is_in_check(Side::Red)) {
        score -= 60;
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

const TranspositionEntry* probe_entry(const SearchContext& context, std::uint64_t key) {
    const TranspositionEntry& entry = context.table[key % context.table.size()];
    if (!entry.occupied || entry.key != key) {
        return nullptr;
    }

    return &entry;
}

void store_entry(
    SearchContext& context,
    std::uint64_t key,
    int depth,
    int score,
    BoundType bound,
    const Move& best_move) {
    TranspositionEntry& entry = context.table[key % context.table.size()];
    if (entry.occupied && entry.depth > depth) {
        return;
    }

    entry.key = key;
    entry.depth = depth;
    entry.score = score;
    entry.bound = bound;
    entry.best_move = best_move;
    entry.occupied = true;
}

void note_killer(SearchContext& context, const Move& move, int ply) {
    if (ply < 0 || ply >= kMaxPly) {
        return;
    }

    if (context.killer_valid[ply][0] && context.killer_moves[ply][0] == move) {
        return;
    }

    context.killer_moves[ply][1] = context.killer_moves[ply][0];
    context.killer_valid[ply][1] = context.killer_valid[ply][0];
    context.killer_moves[ply][0] = move;
    context.killer_valid[ply][0] = true;
}

void note_history(SearchContext& context, const Move& move, int depth) {
    context.history_scores[move_index(move)] += depth * depth;
}

int history_score(const SearchContext& context, const Move& move) {
    return context.history_scores[move_index(move)];
}

void order_moves(
    const GameState& state,
    SearchContext& context,
    std::vector<Move>& moves,
    int ply,
    const Move* hash_move = nullptr) {
    std::stable_sort(moves.begin(), moves.end(), [&state, &context, ply, hash_move](const Move& lhs, const Move& rhs) {
        const auto score_for = [&state, &context, ply, hash_move](const Move& move) {
            if (hash_move != nullptr && move == *hash_move) {
                return 5'000'000;
            }

            int score = move_order_score(state, move) * 100;
            if (is_capture(state, move)) {
                score += 50'000;
            }
            if (ply >= 0 && ply < kMaxPly) {
                if (context.killer_valid[ply][0] && context.killer_moves[ply][0] == move) {
                    score += 40'000;
                } else if (context.killer_valid[ply][1] && context.killer_moves[ply][1] == move) {
                    score += 30'000;
                }
            }
            score += history_score(context, move);
            return score;
        };

        return score_for(lhs) > score_for(rhs);
    });
}

int quiescence(const GameState& state, SearchContext& context, int alpha, int beta, int ply);

int negamax(
    const GameState& state,
    SearchContext& context,
    int depth,
    int alpha,
    int beta,
    int ply,
    bool allow_null_move) {
    check_timeout(context);
    if (context.timed_out) {
        return evaluate_for_side_to_move(state);
    }

    const int original_alpha = alpha;
    const std::uint64_t key = compute_hash(state);
    const TranspositionEntry* cached = probe_entry(context, key);
    const Move* hash_move = cached != nullptr ? &cached->best_move : nullptr;

    if (cached != nullptr && cached->depth >= depth) {
        if (cached->bound == BoundType::Exact) {
            return cached->score;
        }
        if (cached->bound == BoundType::Lower && cached->score >= beta) {
            return cached->score;
        }
        if (cached->bound == BoundType::Upper && cached->score <= alpha) {
            return cached->score;
        }
    }

    if (depth == 0) {
        return quiescence(state, context, alpha, beta, ply);
    }

    if (allow_null_move && depth >= 3 && !state.is_in_check(state.side_to_move())
        && has_null_move_material(state, state.side_to_move())) {
        const int static_eval = evaluate_for_side_to_move(state);
        if (static_eval >= beta) {
            const int reduction = 2 + depth / 3;
            const GameState null_state = state.pass_turn();
            const int score = -negamax(
                null_state,
                context,
                std::max(0, depth - reduction - 1),
                -beta,
                -beta + 1,
                ply + 1,
                false);
            if (context.timed_out) {
                return static_eval;
            }
            if (score >= beta) {
                if (depth <= 6) {
                    return beta;
                }

                const int verification_score = negamax(
                    state,
                    context,
                    std::max(0, depth - reduction - 1),
                    beta - 1,
                    beta,
                    ply,
                    false);
                if (context.timed_out) {
                    return static_eval;
                }
                if (verification_score >= beta) {
                    return beta;
                }
            }
        }
    }

    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        return -kMateScore + ply;
    }

    order_moves(state, context, moves, ply, hash_move);

    int best_score = -kInfinityScore;
    Move best_move = moves.front();
    for (const Move& move : moves) {
        GameState next = state;
        if (!next.apply_move(move)) {
            continue;
        }

        const int score = -negamax(next, context, depth - 1, -beta, -alpha, ply + 1, true);
        if (context.timed_out) {
            break;
        }
        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            if (!is_capture(state, move)) {
                note_killer(context, move, ply);
                note_history(context, move, depth);
            }
            break;
        }
    }

    BoundType bound = BoundType::Exact;
    if (best_score <= original_alpha) {
        bound = BoundType::Upper;
    } else if (best_score >= beta) {
        bound = BoundType::Lower;
    }
    store_entry(context, key, depth, best_score, bound, best_move);

    return best_score;
}

int quiescence(const GameState& state, SearchContext& context, int alpha, int beta, int ply) {
    check_timeout(context);
    if (context.timed_out) {
        return evaluate_for_side_to_move(state);
    }

    const int stand_pat = evaluate_for_side_to_move(state);
    if (stand_pat >= beta) {
        return beta;
    }
    alpha = std::max(alpha, stand_pat);

    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        return -kMateScore + ply;
    }

    if (!state.is_in_check(state.side_to_move())) {
        moves.erase(
            std::remove_if(moves.begin(), moves.end(), [&state](const Move& move) {
                return !is_capture(state, move);
            }),
            moves.end());
    }

    if (moves.empty()) {
        return alpha;
    }

    order_moves(state, context, moves, ply);
    for (const Move& move : moves) {
        GameState next = state;
        if (!next.apply_move(move)) {
            continue;
        }

        const int score = -quiescence(next, context, -beta, -alpha, ply + 1);
        if (context.timed_out) {
            break;
        }
        if (score >= beta) {
            return beta;
        }
        alpha = std::max(alpha, score);
    }

    return alpha;
}

std::optional<Move> fallback_best_move(const GameState& state) {
    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        return std::nullopt;
    }

    std::stable_sort(moves.begin(), moves.end(), [&state](const Move& lhs, const Move& rhs) {
        return move_order_score(state, lhs) > move_order_score(state, rhs);
    });
    return moves.front();
}

RootSearchResult search_root(
    const GameState& state,
    SearchContext& context,
    int depth,
    int alpha,
    int beta) {
    std::vector<Move> moves = generate_all_legal_moves(state);
    if (moves.empty()) {
        throw std::runtime_error("No legal moves available");
    }

    const std::uint64_t key = compute_hash(state);
    const TranspositionEntry* cached = probe_entry(context, key);
    const Move* hash_move = cached != nullptr ? &cached->best_move : nullptr;
    order_moves(state, context, moves, 0, hash_move);

    Move best_move = moves.front();
    int best_score = -kInfinityScore;
    const int original_alpha = alpha;

    for (std::size_t index = 0; index < moves.size(); ++index) {
        const Move& move = moves[index];
        GameState next = state;
        if (!next.apply_move(move)) {
            continue;
        }

        int score;
        if (index == 0) {
            score = -negamax(next, context, depth - 1, -beta, -alpha, 1, true);
        } else {
            score = -negamax(next, context, depth - 1, -alpha - 1, -alpha, 1, true);
            if (!context.timed_out && score > alpha && score < beta) {
                score = -negamax(next, context, depth - 1, -beta, -alpha, 1, true);
            }
        }
        if (context.timed_out) {
            break;
        }
        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        alpha = std::max(alpha, score);
    }

    BoundType bound = BoundType::Exact;
    if (best_score <= original_alpha) {
        bound = BoundType::Upper;
    } else if (best_score >= beta) {
        bound = BoundType::Lower;
    }
    store_entry(context, key, depth, best_score, bound, best_move);
    return RootSearchResult {
        .best_move = best_move,
        .best_score = best_score,
    };
}

SearchContext make_context(const SearchOptions& options) {
    SearchContext context;
    context.options = options;
    if (options.time_budget_ms > 0) {
        context.has_deadline = true;
        context.deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(options.time_budget_ms);
    }
    return context;
}

}  // namespace

SearchResult search_best_move(const GameState& state, const SearchOptions& options) {
    if (options.max_depth <= 0) {
        throw std::invalid_argument("Max depth must be positive");
    }
    if (options.time_budget_ms < 0) {
        throw std::invalid_argument("Time budget must be non-negative");
    }

    const auto started_at = std::chrono::steady_clock::now();
    if (const std::optional<Move> book_move = find_opening_book_move(state); book_move.has_value()) {
        SearchResult result;
        result.best_move = *book_move;
        result.principal_variation.push_back(*book_move);
        result.elapsed_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at)
                .count());
        return result;
    }

    SearchContext context = make_context(options);
    SearchResult result;

    for (int depth = 1; depth <= options.max_depth; ++depth) {
        context.timed_out = false;
        RootSearchResult candidate {};
        if (depth >= 2 && result.best_move.has_value()) {
            int window = kInitialAspirationWindow;
            while (true) {
                const int alpha = std::max(-kInfinityScore, result.score - window);
                const int beta = std::min(kInfinityScore, result.score + window);
                candidate = search_root(state, context, depth, alpha, beta);
                if (context.timed_out) {
                    break;
                }
                if (candidate.best_score <= alpha || candidate.best_score >= beta) {
                    window *= 2;
                    if (window >= kInfinityScore / 2) {
                        candidate = search_root(state, context, depth, -kInfinityScore, kInfinityScore);
                        break;
                    }
                    continue;
                }
                break;
            }
        } else {
            candidate = search_root(state, context, depth, -kInfinityScore, kInfinityScore);
        }
        if (context.timed_out) {
            result.timed_out = true;
            break;
        }

        result.best_move = candidate.best_move;
        result.score = candidate.best_score;
        result.completed_depth = depth;
    }

    if (!result.best_move.has_value()) {
        result.best_move = fallback_best_move(state);
    }

    if (result.best_move.has_value() && result.completed_depth == 0) {
        result.completed_depth = 1;
    }

    result.visited_nodes = context.visited_nodes;
    result.elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at)
            .count());

    GameState pv_state = state;
    const int pv_limit = std::max(result.completed_depth, 1);
    for (int index = 0; index < pv_limit; ++index) {
        const TranspositionEntry* entry = probe_entry(context, compute_hash(pv_state));
        if (entry == nullptr || !entry->occupied) {
            break;
        }
        if (!pv_state.apply_move(entry->best_move)) {
            break;
        }
        result.principal_variation.push_back(entry->best_move);
    }

    if (result.principal_variation.empty() && result.best_move.has_value()) {
        result.principal_variation.push_back(*result.best_move);
    }

    return result;
}

std::optional<Move> try_find_best_move(const GameState& state, int depth) {
    return try_find_best_move(state, SearchOptions {.max_depth = depth, .time_budget_ms = 0});
}

std::optional<Move> try_find_best_move(const GameState& state, const SearchOptions& options) {
    return search_best_move(state, options).best_move;
}

Move find_best_move(const GameState& state, int depth) {
    return find_best_move(state, SearchOptions {.max_depth = depth, .time_budget_ms = 0});
}

Move find_best_move(const GameState& state, const SearchOptions& options) {
    const std::optional<Move> best_move = try_find_best_move(state, options);
    if (!best_move.has_value()) {
        throw std::runtime_error("No legal moves available");
    }

    return *best_move;
}

}  // namespace chinese_chess::engine
