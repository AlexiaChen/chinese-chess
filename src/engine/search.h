#pragma once

#include <optional>

#include "core/game.h"

namespace chinese_chess::engine {

std::optional<Move> try_find_best_move(const GameState& state, int depth);
Move find_best_move(const GameState& state, int depth);

}  // namespace chinese_chess::engine
