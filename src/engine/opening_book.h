#pragma once

#include <optional>

#include "core/game.h"

namespace chinese_chess::engine {

std::optional<Move> find_opening_book_move(const GameState& state);

}  // namespace chinese_chess::engine
