#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"

namespace chinese_chess::bridge {

struct AiMoveReport {
    std::string move;
    int score {};
    int completed_depth {};
    std::uint64_t visited_nodes {};
    int elapsed_ms {};
    std::vector<std::string> principal_variation;
    bool timed_out {false};
};

class BrowserSession {
public:
    BrowserSession();

    [[nodiscard]] std::string current_fen() const;
    [[nodiscard]] Side side_to_move() const;
    [[nodiscard]] std::vector<std::string> legal_moves_from(std::string_view square) const;

    bool apply_move(std::string_view move);
    [[nodiscard]] std::string apply_ai_move(int depth);
    [[nodiscard]] std::string apply_ai_move_with_limits(int max_depth, int time_budget_ms);
    [[nodiscard]] AiMoveReport apply_ai_move_with_report(int max_depth, int time_budget_ms);
    void reset();

private:
    GameState state_;
};

}  // namespace chinese_chess::bridge
