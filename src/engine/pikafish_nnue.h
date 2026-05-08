#pragma once

#include <optional>

#include "core/game.h"

namespace chinese_chess::engine {

std::optional<int> try_evaluate_position_with_pikafish_nnue(const GameState& state);

}  // namespace chinese_chess::engine
