#include "bridge/browser_session.h"

#include <stdexcept>

#include "engine/search.h"
#include "engine/uci_codec.h"

namespace chinese_chess::bridge {

namespace {

AiMoveReport to_ai_move_report(const engine::SearchResult& search_result) {
    if (!search_result.best_move.has_value()) {
        return {};
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

}  // namespace

BrowserSession::BrowserSession() :
    state_(GameState::initial()) {}

AiMoveReport BrowserSession::search_ai_move_for_fen(
    std::string_view fen,
    int max_depth,
    int time_budget_ms) {
    const GameState state = GameState::from_fen(fen);
    return to_ai_move_report(engine::search_best_move(
        state,
        engine::SearchOptions {
            .max_depth = max_depth,
            .time_budget_ms = time_budget_ms,
        }));
}

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
    return apply_move_with_history(engine::from_uci_move(move));
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

    if (!apply_move_with_history(*search_result.best_move)) {
        throw std::runtime_error("AI selected an illegal move");
    }

    return to_ai_move_report(search_result);
}

bool BrowserSession::undo_last_move() {
    if (undo_history_.empty()) {
        return false;
    }

    state_ = undo_history_.back();
    undo_history_.pop_back();
    return true;
}

std::size_t BrowserSession::undo_count() const {
    return undo_history_.size();
}

void BrowserSession::reset() {
    state_ = GameState::initial();
    undo_history_.clear();
}

bool BrowserSession::apply_move_with_history(const Move& move) {
    GameState next_state = state_;
    if (!next_state.apply_move(move)) {
        return false;
    }

    undo_history_.push_back(state_);
    state_ = next_state;
    return true;
}

}  // namespace chinese_chess::bridge
