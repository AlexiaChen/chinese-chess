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
    const std::optional<Move> best_move = engine::try_find_best_move(state_, depth);
    if (!best_move.has_value()) {
        return {};
    }

    if (!state_.apply_move(*best_move)) {
        throw std::runtime_error("AI selected an illegal move");
    }

    return engine::to_uci_move(*best_move);
}

void BrowserSession::reset() {
    state_ = GameState::initial();
}

}  // namespace chinese_chess::bridge
