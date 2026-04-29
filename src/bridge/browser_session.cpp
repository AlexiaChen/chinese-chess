#include "bridge/browser_session.h"

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

void BrowserSession::reset() {
    state_ = GameState::initial();
}

}  // namespace chinese_chess::bridge
