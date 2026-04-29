#pragma once

#include <string>
#include <string_view>

#include "core/game.h"

namespace chinese_chess::engine {

std::string to_uci_square(Position position);
Position from_uci_square(std::string_view square);

std::string to_uci_move(const Move& move);
Move from_uci_move(std::string_view move);

}  // namespace chinese_chess::engine
