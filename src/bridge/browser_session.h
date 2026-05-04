#pragma once

#include <cstdint>
#include <cstddef>
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

struct PositionStatus {
    Side side_to_move {Side::Red};
    bool in_check {false};
    bool has_legal_moves {true};
};

class BrowserSession {
public:
    BrowserSession();

    [[nodiscard]] static AiMoveReport search_ai_move_for_fen(
        std::string_view fen,
        int max_depth,
        int time_budget_ms);

    [[nodiscard]] std::string current_fen() const;
    [[nodiscard]] Side side_to_move() const;
    [[nodiscard]] PositionStatus current_position_status() const;
    [[nodiscard]] std::vector<std::string> legal_moves_from(std::string_view square) const;

    bool apply_move(std::string_view move);
    [[nodiscard]] std::string apply_ai_move(int depth);
    [[nodiscard]] std::string apply_ai_move_with_limits(int max_depth, int time_budget_ms);
    [[nodiscard]] AiMoveReport apply_ai_move_with_report(int max_depth, int time_budget_ms);
    bool undo_last_move();
    [[nodiscard]] std::size_t undo_count() const;
    void reset();

private:
    bool apply_move_with_history(const Move& move);

    GameState state_;
    std::vector<GameState> undo_history_;
};

}  // namespace chinese_chess::bridge
