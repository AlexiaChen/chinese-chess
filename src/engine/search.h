#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/game.h"

namespace chinese_chess::engine {

struct SearchOptions {
    int max_depth {1};
    int time_budget_ms {0};
};

struct SearchResult {
    std::optional<Move> best_move;
    int score {};
    int completed_depth {};
    std::uint64_t visited_nodes {};
    int elapsed_ms {};
    std::vector<Move> principal_variation;
    bool timed_out {false};
};

SearchResult search_best_move(const GameState& state, const SearchOptions& options);
std::optional<Move> try_find_best_move(const GameState& state, int depth);
std::optional<Move> try_find_best_move(const GameState& state, const SearchOptions& options);
Move find_best_move(const GameState& state, int depth);
Move find_best_move(const GameState& state, const SearchOptions& options);

}  // namespace chinese_chess::engine
