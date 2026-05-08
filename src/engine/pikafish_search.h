#pragma once

#include <optional>

#include "engine/search.h"

namespace chinese_chess::engine {

std::optional<SearchResult> try_search_best_move_with_pikafish(
    const GameState& state,
    const SearchOptions& options);

}  // namespace chinese_chess::engine
