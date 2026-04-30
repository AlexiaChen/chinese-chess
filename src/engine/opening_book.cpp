#include "engine/opening_book.h"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/uci_codec.h"

namespace chinese_chess::engine {
namespace {

struct BookLine {
    std::array<std::string_view, 2> history {};
    std::size_t history_length {};
    std::string_view reply;
};

struct ResolvedBookEntry {
    std::string fen;
    Move move;
};

constexpr std::array<BookLine, 5> kBookLines {{
    BookLine {.history = {}, .history_length = 0, .reply = "b2e2"},
    BookLine {.history = {"b2e2", "h9g7"}, .history_length = 2, .reply = "h0g2"},
    BookLine {.history = {"b2e2", "b9c7"}, .history_length = 2, .reply = "b0c2"},
    BookLine {.history = {"b2e2", "h7e7"}, .history_length = 2, .reply = "h0g2"},
    BookLine {.history = {"b2e2", "b7e7"}, .history_length = 2, .reply = "h0g2"},
}};

const std::vector<ResolvedBookEntry>& book_entries() {
    static const std::vector<ResolvedBookEntry> entries = [] {
        std::vector<ResolvedBookEntry> resolved;
        resolved.reserve(kBookLines.size());

        for (const BookLine& line : kBookLines) {
            GameState state = GameState::initial();
            for (std::size_t index = 0; index < line.history_length; ++index) {
                if (!state.apply_move(from_uci_move(line.history[index]))) {
                    throw std::runtime_error("Opening book contains an illegal history move");
                }
            }

            const Move reply = from_uci_move(line.reply);
            if (!state.is_legal_move(reply)) {
                throw std::runtime_error("Opening book contains an illegal reply");
            }

            resolved.push_back(ResolvedBookEntry {
                .fen = state.to_fen(),
                .move = reply,
            });
        }

        return resolved;
    }();

    return entries;
}

}  // namespace

std::optional<Move> find_opening_book_move(const GameState& state) {
    const std::string current_fen = state.to_fen();
    for (const ResolvedBookEntry& entry : book_entries()) {
        if (entry.fen == current_fen) {
            return entry.move;
        }
    }

    return std::nullopt;
}

}  // namespace chinese_chess::engine
