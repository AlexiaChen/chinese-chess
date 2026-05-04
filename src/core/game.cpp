#include "core/game.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace chinese_chess {

namespace {

constexpr std::string_view kInitialFen =
    "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1";

bool is_orthogonal_step(Position from, Position to) {
    return (std::abs(from.file - to.file) == 1 && from.rank == to.rank)
        || (std::abs(from.rank - to.rank) == 1 && from.file == to.file);
}

bool is_diagonal_step(Position from, Position to) {
    return std::abs(from.file - to.file) == 1 && std::abs(from.rank - to.rank) == 1;
}

bool same_axis(Position from, Position to) {
    return from.file == to.file || from.rank == to.rank;
}

}  // namespace

bool Piece::is_empty() const {
    return type == PieceType::None;
}

char Piece::fen_symbol() const {
    if (is_empty()) {
        return '1';
    }

    char symbol = '\0';
    switch (type) {
    case PieceType::General:
        symbol = 'k';
        break;
    case PieceType::Advisor:
        symbol = 'a';
        break;
    case PieceType::Elephant:
        symbol = 'b';
        break;
    case PieceType::Horse:
        symbol = 'n';
        break;
    case PieceType::Rook:
        symbol = 'r';
        break;
    case PieceType::Cannon:
        symbol = 'c';
        break;
    case PieceType::Soldier:
        symbol = 'p';
        break;
    case PieceType::None:
        return '1';
    }

    return side == Side::Red ? static_cast<char>(std::toupper(symbol)) : symbol;
}

Piece Piece::from_fen(char symbol) {
    if (std::isdigit(static_cast<unsigned char>(symbol))) {
        return {};
    }

    const bool is_red = std::isupper(static_cast<unsigned char>(symbol));
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)));

    PieceType type = PieceType::None;
    switch (lower) {
    case 'k':
        type = PieceType::General;
        break;
    case 'a':
        type = PieceType::Advisor;
        break;
    case 'e':
    case 'b':
        type = PieceType::Elephant;
        break;
    case 'h':
    case 'n':
        type = PieceType::Horse;
        break;
    case 'r':
        type = PieceType::Rook;
        break;
    case 'c':
        type = PieceType::Cannon;
        break;
    case 'p':
        type = PieceType::Soldier;
        break;
    default:
        throw std::invalid_argument("Unsupported FEN piece");
    }

    return Piece {type, is_red ? Side::Red : Side::Black};
}

bool Position::is_valid() const {
    return file >= 0 && file < kBoardFiles && rank >= 0 && rank < kBoardRanks;
}

GameState::GameState() {
    board_.fill({});
}

GameState GameState::from_fen(std::string_view fen) {
    std::string fen_text(fen);
    std::istringstream stream(fen_text);

    std::string board_part;
    std::string side_part;
    stream >> board_part >> side_part;

    if (board_part.empty()) {
        throw std::invalid_argument("FEN board is empty");
    }

    GameState state;
    int rank = 0;
    int file = 0;

    for (char symbol : board_part) {
        if (symbol == '/') {
            if (file != kBoardFiles) {
                throw std::invalid_argument("Invalid FEN row width");
            }

            ++rank;
            file = 0;
            continue;
        }

        if (rank >= kBoardRanks) {
            throw std::invalid_argument("Too many FEN ranks");
        }

        if (std::isdigit(static_cast<unsigned char>(symbol))) {
            file += symbol - '0';
            continue;
        }

        if (file >= kBoardFiles) {
            throw std::invalid_argument("Too many FEN files");
        }

        state.set_piece(Position {file, rank}, Piece::from_fen(symbol));
        ++file;
    }

    if (rank != kBoardRanks - 1 || file != kBoardFiles) {
        throw std::invalid_argument("FEN does not contain 10 complete ranks");
    }

    if (!side_part.empty()) {
        if (side_part == "w") {
            state.side_to_move_ = Side::Red;
        } else if (side_part == "b") {
            state.side_to_move_ = Side::Black;
        } else {
            throw std::invalid_argument("Unsupported side to move token");
        }
    }

    return state;
}

GameState GameState::initial() {
    return from_fen(kInitialFen);
}

std::string GameState::to_fen() const {
    std::ostringstream output;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        int empty_count = 0;

        for (int file = 0; file < kBoardFiles; ++file) {
            const Piece piece = piece_at(Position {file, rank});
            if (piece.is_empty()) {
                ++empty_count;
                continue;
            }

            if (empty_count > 0) {
                output << empty_count;
                empty_count = 0;
            }

            output << piece.fen_symbol();
        }

        if (empty_count > 0) {
            output << empty_count;
        }

        if (rank + 1 < kBoardRanks) {
            output << '/';
        }
    }

    output << ' ' << (side_to_move_ == Side::Red ? 'w' : 'b') << " - - 0 1";
    return output.str();
}

std::string GameState::debug_string() const {
    std::ostringstream output;

    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Piece piece = piece_at(Position {file, rank});
            output << (piece.is_empty() ? '.' : piece.fen_symbol());
            if (file + 1 < kBoardFiles) {
                output << ' ';
            }
        }
        output << '\n';
    }

    output << "side_to_move="
           << (side_to_move_ == Side::Red ? "red" : "black")
           << '\n';
    return output.str();
}

Side GameState::side_to_move() const {
    return side_to_move_;
}

Piece GameState::piece_at(Position position) const {
    if (!position.is_valid()) {
        throw std::out_of_range("Board lookup outside board");
    }

    return board_[index(position)];
}

std::vector<Move> GameState::generate_pseudo_legal_moves(Position from) const {
    if (!from.is_valid()) {
        return {};
    }

    const Piece piece = piece_at(from);
    if (piece.is_empty() || piece.side != side_to_move_) {
        return {};
    }

    auto append_if_valid = [this, &piece, &from](std::vector<Move>& moves, Position to) {
        if (!to.is_valid()) {
            return;
        }

        const Piece target = piece_at(to);
        if (!target.is_empty() && target.side == piece.side) {
            return;
        }

        moves.push_back(Move {from, to});
    };

    std::vector<Move> moves;
    switch (piece.type) {
    case PieceType::General: {
        for (const Position to : {
                 Position {from.file + 1, from.rank},
                 Position {from.file - 1, from.rank},
                 Position {from.file, from.rank + 1},
                 Position {from.file, from.rank - 1},
             }) {
            if (to.is_valid() && in_palace(to, piece.side)) {
                append_if_valid(moves, to);
            }
        }

        for (int rank = from.rank + (piece.side == Side::Red ? -1 : 1);
             rank >= 0 && rank < kBoardRanks;
             rank += (piece.side == Side::Red ? -1 : 1)) {
            const Position scan {from.file, rank};
            const Piece target = piece_at(scan);
            if (target.is_empty()) {
                continue;
            }

            if (target.type == PieceType::General && target.side != piece.side) {
                moves.push_back(Move {from, scan});
            }
            break;
        }
        break;
    }
    case PieceType::Advisor:
        for (const Position to : {
                 Position {from.file + 1, from.rank + 1},
                 Position {from.file + 1, from.rank - 1},
                 Position {from.file - 1, from.rank + 1},
                 Position {from.file - 1, from.rank - 1},
             }) {
            if (to.is_valid() && in_palace(to, piece.side)) {
                append_if_valid(moves, to);
            }
        }
        break;
    case PieceType::Elephant:
        for (const Position to : {
                 Position {from.file + 2, from.rank + 2},
                 Position {from.file + 2, from.rank - 2},
                 Position {from.file - 2, from.rank + 2},
                 Position {from.file - 2, from.rank - 2},
             }) {
            const Position midpoint {
                (from.file + to.file) / 2,
                (from.rank + to.rank) / 2,
            };
            if (!to.is_valid() || crossed_river(to, piece.side)) {
                continue;
            }
            if (!piece_at(midpoint).is_empty()) {
                continue;
            }
            append_if_valid(moves, to);
        }
        break;
    case PieceType::Horse: {
        struct Candidate {
            Position to;
            Position leg;
        };
        for (const Candidate candidate : {
                 Candidate {Position {from.file + 1, from.rank + 2}, Position {from.file, from.rank + 1}},
                 Candidate {Position {from.file - 1, from.rank + 2}, Position {from.file, from.rank + 1}},
                 Candidate {Position {from.file + 1, from.rank - 2}, Position {from.file, from.rank - 1}},
                 Candidate {Position {from.file - 1, from.rank - 2}, Position {from.file, from.rank - 1}},
                 Candidate {Position {from.file + 2, from.rank + 1}, Position {from.file + 1, from.rank}},
                 Candidate {Position {from.file + 2, from.rank - 1}, Position {from.file + 1, from.rank}},
                 Candidate {Position {from.file - 2, from.rank + 1}, Position {from.file - 1, from.rank}},
                 Candidate {Position {from.file - 2, from.rank - 1}, Position {from.file - 1, from.rank}},
             }) {
            if (!candidate.to.is_valid() || !candidate.leg.is_valid()) {
                continue;
            }
            if (!piece_at(candidate.leg).is_empty()) {
                continue;
            }
            append_if_valid(moves, candidate.to);
        }
        break;
    }
    case PieceType::Rook:
        for (const Position direction : {
                 Position {1, 0},
                 Position {-1, 0},
                 Position {0, 1},
                 Position {0, -1},
             }) {
            for (Position to {from.file + direction.file, from.rank + direction.rank};
                 to.is_valid();
                 to = Position {to.file + direction.file, to.rank + direction.rank}) {
                const Piece target = piece_at(to);
                if (target.is_empty()) {
                    moves.push_back(Move {from, to});
                    continue;
                }

                if (target.side != piece.side) {
                    moves.push_back(Move {from, to});
                }
                break;
            }
        }
        break;
    case PieceType::Cannon:
        for (const Position direction : {
                 Position {1, 0},
                 Position {-1, 0},
                 Position {0, 1},
                 Position {0, -1},
             }) {
            bool screen_seen = false;
            for (Position to {from.file + direction.file, from.rank + direction.rank};
                 to.is_valid();
                 to = Position {to.file + direction.file, to.rank + direction.rank}) {
                const Piece target = piece_at(to);
                if (!screen_seen) {
                    if (target.is_empty()) {
                        moves.push_back(Move {from, to});
                        continue;
                    }
                    screen_seen = true;
                    continue;
                }

                if (!target.is_empty()) {
                    if (target.side != piece.side) {
                        moves.push_back(Move {from, to});
                    }
                    break;
                }
            }
        }
        break;
    case PieceType::Soldier: {
        const int forward = piece.side == Side::Red ? -1 : 1;
        append_if_valid(moves, Position {from.file, from.rank + forward});
        if (crossed_river(from, piece.side)) {
            append_if_valid(moves, Position {from.file + 1, from.rank});
            append_if_valid(moves, Position {from.file - 1, from.rank});
        }
        break;
    }
    case PieceType::None:
        break;
    }

    return moves;
}

std::vector<Move> GameState::generate_legal_moves(Position from) const {
    const std::vector<Move> pseudo = generate_pseudo_legal_moves(from);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());

    for (const Move& move : pseudo) {
        if (!would_leave_self_in_check(move)) {
            legal.push_back(move);
        }
    }

    return legal;
}

bool GameState::has_any_legal_moves() const {
    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position from {file, rank};
            const Piece piece = piece_at(from);
            if (piece.is_empty() || piece.side != side_to_move_) {
                continue;
            }

            if (!generate_legal_moves(from).empty()) {
                return true;
            }
        }
    }

    return false;
}

bool GameState::is_legal_move(const Move& move) const {
    const std::vector<Move> legal_moves = generate_legal_moves(move.from);
    return std::ranges::find(legal_moves, move) != legal_moves.end();
}

bool GameState::is_in_check(Side side) const {
    return is_square_attacked(find_general(side), opposite(side));
}

bool GameState::generals_facing() const {
    const Position red_general = find_general(Side::Red);
    const Position black_general = find_general(Side::Black);
    if (red_general.file != black_general.file) {
        return false;
    }

    return blocker_count(red_general, black_general) == 0;
}

GameState GameState::pass_turn() const {
    GameState next = *this;
    next.side_to_move_ = opposite(side_to_move_);
    return next;
}

bool GameState::apply_move(const Move& move) {
    if (!is_legal_move(move)) {
        return false;
    }

    apply_move_unchecked(move);
    return true;
}

int GameState::index(Position position) {
    return position.rank * kBoardFiles + position.file;
}

bool GameState::in_palace(Position position, Side side) {
    const bool file_ok = position.file >= 3 && position.file <= 5;
    const bool rank_ok = side == Side::Red
        ? position.rank >= 7 && position.rank <= 9
        : position.rank >= 0 && position.rank <= 2;
    return file_ok && rank_ok;
}

bool GameState::crossed_river(Position position, Side side) {
    return side == Side::Red ? position.rank <= 4 : position.rank >= 5;
}

Side GameState::opposite(Side side) {
    return side == Side::Red ? Side::Black : Side::Red;
}

Position GameState::find_general(Side side) const {
    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position position {file, rank};
            const Piece piece = piece_at(position);
            if (piece.type == PieceType::General && piece.side == side) {
                return position;
            }
        }
    }

    throw std::runtime_error("General not found");
}

bool GameState::is_square_attacked(Position target, Side attacker) const {
    for (int rank = 0; rank < kBoardRanks; ++rank) {
        for (int file = 0; file < kBoardFiles; ++file) {
            const Position from {file, rank};
            const Piece piece = piece_at(from);
            if (piece.is_empty() || piece.side != attacker) {
                continue;
            }
            if (piece_attacks_square(from, target, piece)) {
                return true;
            }
        }
    }

    return false;
}

bool GameState::piece_attacks_square(Position from, Position target, Piece piece) const {
    if (from == target) {
        return false;
    }

    const Piece target_piece = piece_at(target);
    if (!target_piece.is_empty() && target_piece.side == piece.side) {
        return false;
    }

    switch (piece.type) {
    case PieceType::General:
        if (is_orthogonal_step(from, target) && in_palace(target, piece.side)) {
            return true;
        }
        return from.file == target.file
            && target_piece.type == PieceType::General
            && blocker_count(from, target) == 0;
    case PieceType::Advisor:
        return is_diagonal_step(from, target) && in_palace(target, piece.side);
    case PieceType::Elephant: {
        if (std::abs(from.file - target.file) != 2 || std::abs(from.rank - target.rank) != 2) {
            return false;
        }
        if (crossed_river(target, piece.side)) {
            return false;
        }
        const Position midpoint {(from.file + target.file) / 2, (from.rank + target.rank) / 2};
        return piece_at(midpoint).is_empty();
    }
    case PieceType::Horse: {
        const int file_delta = std::abs(from.file - target.file);
        const int rank_delta = std::abs(from.rank - target.rank);
        if (!((file_delta == 1 && rank_delta == 2) || (file_delta == 2 && rank_delta == 1))) {
            return false;
        }

        const Position leg = file_delta == 1
            ? Position {from.file, from.rank + (target.rank > from.rank ? 1 : -1)}
            : Position {from.file + (target.file > from.file ? 1 : -1), from.rank};
        return piece_at(leg).is_empty();
    }
    case PieceType::Rook:
        return same_axis(from, target) && blocker_count(from, target) == 0;
    case PieceType::Cannon:
        return same_axis(from, target)
            && !target_piece.is_empty()
            && blocker_count(from, target) == 1;
    case PieceType::Soldier: {
        const int forward = piece.side == Side::Red ? -1 : 1;
        if (target.file == from.file && target.rank == from.rank + forward) {
            return true;
        }
        if (crossed_river(from, piece.side)
            && target.rank == from.rank
            && std::abs(target.file - from.file) == 1) {
            return true;
        }
        return false;
    }
    case PieceType::None:
        return false;
    }

    return false;
}

bool GameState::path_clear(Position from, Position to) const {
    return blocker_count(from, to) == 0;
}

int GameState::blocker_count(Position from, Position to) const {
    if (!same_axis(from, to)) {
        throw std::invalid_argument("Blocker count requires orthogonal path");
    }

    const int file_step = (to.file > from.file) - (to.file < from.file);
    const int rank_step = (to.rank > from.rank) - (to.rank < from.rank);

    int count = 0;
    for (Position scan {from.file + file_step, from.rank + rank_step};
         scan != to;
         scan = Position {scan.file + file_step, scan.rank + rank_step}) {
        if (!piece_at(scan).is_empty()) {
            ++count;
        }
    }

    return count;
}

bool GameState::would_leave_self_in_check(const Move& move) const {
    const Piece piece = piece_at(move.from);
    if (piece.is_empty()) {
        return true;
    }

    GameState next = *this;
    next.apply_move_unchecked(move);
    return next.is_in_check(piece.side);
}

void GameState::set_piece(Position position, Piece piece) {
    board_[index(position)] = piece;
}

void GameState::apply_move_unchecked(const Move& move) {
    const Piece moving_piece = piece_at(move.from);
    set_piece(move.to, moving_piece);
    set_piece(move.from, {});
    side_to_move_ = opposite(side_to_move_);
}

}  // namespace chinese_chess
