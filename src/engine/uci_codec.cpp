#include "engine/uci_codec.h"

#include <cctype>
#include <stdexcept>

namespace chinese_chess::engine {

std::string to_uci_square(Position position) {
    if (!position.is_valid()) {
        throw std::invalid_argument("Invalid board position");
    }

    const char file = static_cast<char>('a' + position.file);
    const char rank = static_cast<char>('0' + (kBoardRanks - 1 - position.rank));
    return std::string {file, rank};
}

Position from_uci_square(std::string_view square) {
    if (square.size() != 2) {
        throw std::invalid_argument("UCI square must be 2 characters");
    }

    const char file_char = static_cast<char>(std::tolower(static_cast<unsigned char>(square[0])));
    const char rank_char = square[1];
    if (file_char < 'a' || file_char > 'i' || rank_char < '0' || rank_char > '9') {
        throw std::invalid_argument("UCI square out of range");
    }

    return Position {
        file_char - 'a',
        kBoardRanks - 1 - (rank_char - '0'),
    };
}

std::string to_uci_move(const Move& move) {
    return to_uci_square(move.from) + to_uci_square(move.to);
}

Move from_uci_move(std::string_view move) {
    if (move.size() != 4) {
        throw std::invalid_argument("UCI move must be 4 characters");
    }

    return Move {
        from_uci_square(move.substr(0, 2)),
        from_uci_square(move.substr(2, 2)),
    };
}

}  // namespace chinese_chess::engine
