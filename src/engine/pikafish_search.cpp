#include "engine/pikafish_search.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../../third_party/pikafish/src/engine.h"
#include "../../third_party/pikafish/src/misc.h"
#include "../../third_party/pikafish/src/bitboard.h"
#include "../../third_party/pikafish/src/position.h"
#include "../../third_party/pikafish/src/score.h"
#include "../../third_party/pikafish/src/uci.h"
#include "engine/uci_codec.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, emit_pikafish_search_progress_js, (
    const char* move,
    int depth,
    int analyzed_root_moves,
    int total_root_moves,
    double visited_nodes), {
    if (typeof globalThis.__chineseChessHandleSearchProgress === 'function') {
        globalThis.__chineseChessHandleSearchProgress(
            UTF8ToString(move),
            depth,
            analyzed_root_moves,
            total_root_moves,
            visited_nodes);
        return;
    }
    if (typeof Module.chineseChessHandleSearchProgress === 'function') {
        Module.chineseChessHandleSearchProgress(
            UTF8ToString(move),
            depth,
            analyzed_root_moves,
            total_root_moves,
            visited_nodes);
    }
});

#else
inline void emit_pikafish_search_progress_js(
    const char*,
    int,
    int,
    int,
    double) {}
#endif

namespace chinese_chess::engine {

namespace {

constexpr int kMateScore = 1'000'000;

std::string resolve_engine_binary_path() {
#ifdef __EMSCRIPTEN__
    return "/pikafish";
#else
    return std::string(CHINESE_CHESS_SOURCE_DIR) + "/third_party/pikafish/src/pikafish";
#endif
}

std::string resolve_eval_file_path() {
#ifdef __EMSCRIPTEN__
    return "/pikafish.nnue";
#else
    return std::string(CHINESE_CHESS_SOURCE_DIR) + "/third_party/pikafish/src/pikafish.nnue";
#endif
}

int count_root_moves(const GameState& state) {
    int total = 0;
    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position from {file, rank};
            const Piece piece = state.piece_at(from);
            if (piece.is_empty() || piece.side != state.side_to_move()) {
                continue;
            }
            total += static_cast<int>(state.generate_legal_moves(from).size());
        }
    }
    return total;
}

int to_public_score(const Stockfish::Score& score) {
    return score.visit([](const auto& entry) -> int {
        using Entry = std::decay_t<decltype(entry)>;
        if constexpr (std::is_same_v<Entry, Stockfish::Score::Mate>) {
            return entry.plies >= 0 ? (kMateScore - entry.plies) : (-kMateScore - entry.plies);
        } else {
            return entry.value;
        }
    });
}

std::vector<Move> decode_pv(std::string_view principal_variation) {
    std::vector<Move> moves;
    std::istringstream stream {std::string(principal_variation)};
    std::string token;
    while (stream >> token) {
        if (token == "(none)" || token == "0000") {
            continue;
        }
        moves.push_back(from_uci_move(token));
    }
    return moves;
}

void emit_progress(
    const SearchOptions& options,
    const std::string& current_move,
    int depth,
    int analyzed_root_moves,
    int total_root_moves,
    std::uint64_t visited_nodes,
    const std::optional<Move>& best_move_so_far) {
    if (current_move.empty() || current_move == "(none)" || current_move == "0000") {
        return;
    }

    if (options.progress_callback) {
        options.progress_callback(SearchProgress {
            .depth = depth,
            .current_move = from_uci_move(current_move),
            .best_move_so_far = best_move_so_far,
            .visited_nodes = visited_nodes,
            .analyzed_root_moves = analyzed_root_moves,
            .total_root_moves = total_root_moves,
        });
    }

    emit_pikafish_search_progress_js(
        current_move.c_str(),
        depth,
        analyzed_root_moves,
        total_root_moves,
        static_cast<double>(visited_nodes));
}

class PikafishSearchAdapter {
    public:
    PikafishSearchAdapter() {
        static std::once_flag stockfish_init_once;
        std::call_once(stockfish_init_once, []() {
            Stockfish::Bitboards::init();
            Stockfish::Position::init();
        });
        engine_ = std::make_unique<Stockfish::Engine>(resolve_engine_binary_path());
        auto& options = engine_->get_options();
        std::istringstream eval_file_option("name EvalFile value " + resolve_eval_file_path());
        options.setoption(eval_file_option);
        engine_->set_on_verify_networks([](std::string_view) {});
    }

    std::optional<SearchResult> search(const GameState& state, const SearchOptions& options) {
        std::lock_guard<std::mutex> lock(engine_mutex_);

        if (const auto error = engine_->set_position(state.to_fen(), {}); error.has_value()) {
            return std::nullopt;
        }

        SearchResult result;
        const int total_root_moves = count_root_moves(state);
        std::optional<Move> best_move_so_far;
        std::uint64_t latest_nodes = 0;

        engine_->set_on_update_no_moves([&result](const Stockfish::Engine::InfoShort& info) {
            result.score = to_public_score(info.score);
            result.completed_depth = info.depth;
        });
        engine_->set_on_update_full([&] (const Stockfish::Engine::InfoFull& info) {
            result.score = to_public_score(info.score);
            result.completed_depth = info.depth;
            result.visited_nodes = info.nodes;
            latest_nodes = info.nodes;
            result.elapsed_ms = static_cast<int>(info.timeMs);
            result.principal_variation = decode_pv(info.pv);
            if (!result.principal_variation.empty()) {
                best_move_so_far = result.principal_variation.front();
            }
        });
        engine_->set_on_iter([&](const Stockfish::Engine::InfoIter& info) {
            emit_progress(
                options,
                std::string(info.currmove),
                info.depth,
                static_cast<int>(info.currmovenumber > 0 ? info.currmovenumber - 1 : 0),
                total_root_moves,
                latest_nodes,
                best_move_so_far);
        });
        engine_->set_on_bestmove([&result](std::string_view bestmove, std::string_view) {
            if (bestmove.empty() || bestmove == "(none)" || bestmove == "0000") {
                result.best_move.reset();
                return;
            }
            result.best_move = from_uci_move(std::string(bestmove));
        });

        Stockfish::Search::LimitsType limits;
        limits.startTime = Stockfish::now();
        limits.depth = options.max_depth;
        limits.movetime = options.time_budget_ms > 0 ? options.time_budget_ms : 0;

        const auto started_at = std::chrono::steady_clock::now();
        engine_->go(limits);
        engine_->wait_for_search_finished();

        if (!result.best_move.has_value() && !result.principal_variation.empty()) {
            result.best_move = result.principal_variation.front();
        }

        if (result.elapsed_ms == 0) {
            result.elapsed_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started_at)
                    .count());
        }

        result.timed_out = options.time_budget_ms > 0 && result.completed_depth < options.max_depth;
        return result;
    }

   private:
    std::unique_ptr<Stockfish::Engine> engine_;
    std::mutex engine_mutex_;
};

PikafishSearchAdapter& adapter() {
    static PikafishSearchAdapter instance;
    return instance;
}

}  // namespace

std::optional<SearchResult> try_search_best_move_with_pikafish(
    const GameState& state,
    const SearchOptions& options) {
    return adapter().search(state, options);
}

}  // namespace chinese_chess::engine
