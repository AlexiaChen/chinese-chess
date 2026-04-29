#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"

namespace chinese_chess::bridge {

class BrowserSession {
public:
    BrowserSession();

    [[nodiscard]] std::string current_fen() const;
    [[nodiscard]] Side side_to_move() const;
    [[nodiscard]] std::vector<std::string> legal_moves_from(std::string_view square) const;

    bool apply_move(std::string_view move);
    [[nodiscard]] std::string apply_ai_move(int depth);
    void reset();

private:
    GameState state_;
};

}  // namespace chinese_chess::bridge
