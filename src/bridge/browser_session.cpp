#include "bridge/browser_session.h"

#include <stdexcept>

#include "engine/search.h"
#include "engine/uci_codec.h"

namespace chinese_chess::bridge {

BrowserSession::BrowserSession() :
    state_(GameState::initial()) {}

std::string BrowserSession::current_fen() const {
    return state_.to_fen();
}

Side BrowserSession::side_to_move() const {
    return state_.side_to_move();
}

std::vector<std::string> BrowserSession::legal_moves_from(std::string_view square) const {
    const Position from = engine::from_uci_square(square);
    const std::vector<Move> legal_moves = state_.generate_legal_moves(from);

    std::vector<std::string> encoded;
    encoded.reserve(legal_moves.size());
    for (const Move& move : legal_moves) {
        encoded.push_back(engine::to_uci_move(move));
    }

    return encoded;
}

bool BrowserSession::apply_move(std::string_view move) {
    return state_.apply_move(engine::from_uci_move(move));
}

std::string BrowserSession::apply_ai_move(int depth) {
    return apply_ai_move_with_limits(depth, 0);
}

std::string BrowserSession::apply_ai_move_with_limits(int max_depth, int time_budget_ms) {
    return apply_ai_move_with_report(max_depth, time_budget_ms).move;
}

AiMoveReport BrowserSession::apply_ai_move_with_report(int max_depth, int time_budget_ms) {
    const engine::SearchResult search_result = engine::search_best_move(
        state_,
        engine::SearchOptions {
            .max_depth = max_depth,
            .time_budget_ms = time_budget_ms,
        });
    if (!search_result.best_move.has_value()) {
        return {};
    }

    if (!state_.apply_move(*search_result.best_move)) {
        throw std::runtime_error("AI selected an illegal move");
    }

    AiMoveReport report {
        .move = engine::to_uci_move(*search_result.best_move),
        .score = search_result.score,
        .completed_depth = search_result.completed_depth,
        .visited_nodes = search_result.visited_nodes,
        .elapsed_ms = search_result.elapsed_ms,
        .timed_out = search_result.timed_out,
    };
    report.principal_variation.reserve(search_result.principal_variation.size());
    for (const Move& move : search_result.principal_variation) {
        report.principal_variation.push_back(engine::to_uci_move(move));
    }

    return report;
}

void BrowserSession::reset() {
    state_ = GameState::initial();
}

}  // namespace chinese_chess::bridge
