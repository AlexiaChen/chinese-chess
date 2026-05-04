#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace chinese_chess {

constexpr int kBoardFiles = 9;
constexpr int kBoardRanks = 10;
constexpr int kBoardSquares = kBoardFiles * kBoardRanks;

enum class Side {
    Red,
    Black,
};

enum class PieceType {
    None,
    General,
    Advisor,
    Elephant,
    Horse,
    Rook,
    Cannon,
    Soldier,
};

struct Piece {
    PieceType type {PieceType::None};
    Side side {Side::Red};

    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] char fen_symbol() const;

    static Piece from_fen(char symbol);
};

struct Position {
    int file {};
    int rank {};

    [[nodiscard]] bool is_valid() const;
    auto operator<=>(const Position&) const = default;
};

struct Move {
    Position from {};
    Position to {};

    auto operator<=>(const Move&) const = default;
};

class GameState {
public:
    GameState();

    static GameState from_fen(std::string_view fen);
    static GameState initial();

    [[nodiscard]] std::string to_fen() const;
    [[nodiscard]] std::string debug_string() const;

    [[nodiscard]] Side side_to_move() const;
    [[nodiscard]] Piece piece_at(Position position) const;

    [[nodiscard]] std::vector<Move> generate_pseudo_legal_moves(Position from) const;
    [[nodiscard]] std::vector<Move> generate_legal_moves(Position from) const;
    [[nodiscard]] bool has_any_legal_moves() const;
    [[nodiscard]] bool is_legal_move(const Move& move) const;
    [[nodiscard]] bool is_in_check(Side side) const;
    [[nodiscard]] bool generals_facing() const;
    [[nodiscard]] GameState pass_turn() const;

    bool apply_move(const Move& move);

private:
    std::array<Piece, kBoardSquares> board_ {};
    Side side_to_move_ {Side::Red};

    [[nodiscard]] static int index(Position position);
    [[nodiscard]] static bool in_palace(Position position, Side side);
    [[nodiscard]] static bool crossed_river(Position position, Side side);
    [[nodiscard]] static Side opposite(Side side);

    [[nodiscard]] Position find_general(Side side) const;
    [[nodiscard]] bool is_square_attacked(Position target, Side attacker) const;
    [[nodiscard]] bool piece_attacks_square(Position from, Position target, Piece piece) const;
    [[nodiscard]] bool path_clear(Position from, Position to) const;
    [[nodiscard]] int blocker_count(Position from, Position to) const;
    [[nodiscard]] bool would_leave_self_in_check(const Move& move) const;

    void set_piece(Position position, Piece piece);
    void apply_move_unchecked(const Move& move);
};

}  // namespace chinese_chess
