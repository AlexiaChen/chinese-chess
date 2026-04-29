#include "core/game.h"
#include "bridge/browser_session.h"
#include "engine/pikafish_process.h"
#include "engine/search.h"
#include "engine/uci_codec.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using chinese_chess::GameState;
using chinese_chess::Move;
using chinese_chess::Position;
using chinese_chess::Side;

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

bool contains_move(const std::vector<Move>& moves, Move move) {
    for (const Move& candidate : moves) {
        if (candidate == move) {
            return true;
        }
    }
    return false;
}

void fen_round_trip_test() {
    const GameState game = GameState::initial();
    expect(
        game.to_fen() == "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1",
        "Initial FEN should round-trip");
}

void soldier_before_and_after_river_test() {
    const GameState initial = GameState::initial();
    const auto opening_moves = initial.generate_legal_moves(Position {0, 6});

    expect(opening_moves.size() == 1, "Opening soldier should have one legal move");
    expect(contains_move(opening_moves, Move {Position {0, 6}, Position {0, 5}}),
           "Opening soldier should move forward");

    const GameState crossed = GameState::from_fen("4k4/9/9/9/3P5/4R4/9/9/9/4K4 w");
    const auto crossed_moves = crossed.generate_legal_moves(Position {3, 4});

    expect(crossed_moves.size() == 3, "Crossed-river soldier should have three legal moves");
    expect(contains_move(crossed_moves, Move {Position {3, 4}, Position {3, 3}}),
           "Crossed-river soldier should still move forward");
    expect(contains_move(crossed_moves, Move {Position {3, 4}, Position {2, 4}}),
           "Crossed-river soldier should move left");
    expect(contains_move(crossed_moves, Move {Position {3, 4}, Position {4, 4}}),
           "Crossed-river soldier should move right");
}

void horse_leg_block_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/9/9/4P4/4H4/9/4K4 w");
    const auto moves = game.generate_legal_moves(Position {4, 7});

    expect(!contains_move(moves, Move {Position {4, 7}, Position {3, 5}}),
           "Horse should be blocked by its leg");
    expect(!contains_move(moves, Move {Position {4, 7}, Position {5, 5}}),
           "Horse should be blocked by its leg");
    expect(contains_move(moves, Move {Position {4, 7}, Position {2, 6}}),
           "Horse should keep side jumps");
}

void elephant_river_rule_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/9/4E4/9/9/9/4K4 w");
    const auto moves = game.generate_legal_moves(Position {4, 5});

    expect(!contains_move(moves, Move {Position {4, 5}, Position {2, 3}}),
           "Elephant cannot cross the river");
    expect(!contains_move(moves, Move {Position {4, 5}, Position {6, 3}}),
           "Elephant cannot cross the river");
}

void cannon_capture_rule_test() {
    const GameState game = GameState::from_fen("4k4/9/1r7/9/9/1P2P4/9/1C7/9/4K4 w");
    const auto moves = game.generate_legal_moves(Position {1, 7});

    expect(contains_move(moves, Move {Position {1, 7}, Position {1, 2}}),
           "Cannon should capture over exactly one screen");
    expect(!contains_move(moves, Move {Position {1, 7}, Position {1, 0}}),
           "Cannon cannot capture without exactly one screen");
}

void general_facing_rule_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/9/9/9/4r4/9/4K4 b");
    expect(!game.is_legal_move(Move {Position {4, 7}, Position {3, 7}}),
           "Cannot move a blocker and leave generals facing");
}

void apply_move_switches_side_test() {
    GameState game = GameState::initial();
    expect(game.apply_move(Move {Position {0, 6}, Position {0, 5}}),
           "Opening move should be legal");
    expect(game.side_to_move() == Side::Black, "Side to move should switch after a move");
}

void uci_codec_round_trip_test() {
    expect(chinese_chess::engine::to_uci_square(Position {0, 9}) == "a0",
           "Bottom-left red square should encode to a0");
    expect(chinese_chess::engine::to_uci_square(Position {8, 0}) == "i9",
           "Top-right black square should encode to i9");

    const Move move = chinese_chess::engine::from_uci_move("b2b9");
    expect(move.from == Position {1, 7}, "UCI move should decode source square");
    expect(move.to == Position {1, 0}, "UCI move should decode target square");
    expect(chinese_chess::engine::to_uci_move(move) == "b2b9",
           "UCI move should round-trip");
}

void pikafish_process_adapter_test() {
    const std::filesystem::path fixture =
        std::filesystem::path(CHINESE_CHESS_SOURCE_DIR) / "tests" / "fixtures" / "fake_uci_engine.py";
    chinese_chess::engine::PikafishProcess engine("python3 " + fixture.string());

    engine.start();
    expect(engine.engine_name() == "FakePikafish", "Engine handshake should capture engine name");

    const Move best_move = engine.find_best_move(GameState::initial(), 1);
    expect(best_move.from == Position {0, 6}, "Adapter should decode bestmove source square");
    expect(best_move.to == Position {0, 5}, "Adapter should decode bestmove target square");

    engine.stop();
}

void search_prefers_winning_capture_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/4r4/4R4/9/9/9/4K4 w");
    const Move best_move = chinese_chess::engine::find_best_move(game, 1);

    expect(best_move == Move {Position {4, 5}, Position {4, 4}},
           "Search should prefer capturing a free rook");
}

void time_budgeted_search_returns_playable_move_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/4r4/4R4/9/9/9/4K4 w");
    const auto best_move = chinese_chess::engine::try_find_best_move(
        game,
        chinese_chess::engine::SearchOptions {
            .max_depth = 4,
            .time_budget_ms = 25,
        });

    expect(best_move.has_value(), "Time-budgeted search should still return a move");
    expect(*best_move == Move {Position {4, 5}, Position {4, 4}},
           "Time-budgeted search should keep obvious tactical wins");
}

void search_report_exposes_metadata_test() {
    const GameState game = GameState::from_fen("4k4/9/9/9/4r4/4R4/9/9/9/4K4 w");
    const auto report = chinese_chess::engine::search_best_move(
        game,
        chinese_chess::engine::SearchOptions {
            .max_depth = 4,
            .time_budget_ms = 25,
        });

    expect(report.best_move.has_value(), "Search report should contain a best move");
    expect(*report.best_move == Move {Position {4, 5}, Position {4, 4}},
           "Search report should expose the same tactical best move");
    expect(report.completed_depth >= 1, "Search report should record completed depth");
    expect(report.visited_nodes > 0, "Search report should record visited nodes");
    expect(report.principal_variation.size() >= 1, "Search report should expose a principal variation");
}

void browser_session_bridge_test() {
    chinese_chess::bridge::BrowserSession session;

    expect(
        session.current_fen() == "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1",
        "Browser session should expose the Pikafish-compatible initial FEN");

    const auto moves = session.legal_moves_from("a3");
    expect(std::find(moves.begin(), moves.end(), "a3a4") != moves.end(),
           "Browser session should expose legal opening soldier move");
    expect(session.apply_move("a3a4"), "Browser session should accept a legal move");
    expect(session.side_to_move() == Side::Black, "Browser session should switch side after applying move");

    const std::string ai_move = session.apply_ai_move(1);
    expect(!ai_move.empty(), "Browser session should be able to apply an AI move");
    expect(session.side_to_move() == Side::Red, "AI move should hand control back to the human side");

    session.reset();
    expect(session.apply_move("a3a4"), "Browser session should still accept a legal move after reset");
    const std::string stronger_ai_move = session.apply_ai_move_with_limits(4, 25);
    expect(!stronger_ai_move.empty(), "Budgeted browser AI should still produce a move");
    expect(session.side_to_move() == Side::Red,
           "Budgeted browser AI move should return control to the human side");

    session.reset();
    expect(session.apply_move("a3a4"), "Browser session should accept a legal move before reporting AI metadata");
    const auto ai_report = session.apply_ai_move_with_report(4, 25);
    expect(!ai_report.move.empty(), "Browser session AI report should include the applied move");
    expect(ai_report.completed_depth >= 1, "Browser session AI report should include completed depth");
    expect(ai_report.visited_nodes > 0, "Browser session AI report should include visited nodes");
    expect(!ai_report.principal_variation.empty(),
           "Browser session AI report should include a principal variation");
}

}  // namespace

int main() {
    fen_round_trip_test();
    soldier_before_and_after_river_test();
    horse_leg_block_test();
    elephant_river_rule_test();
    cannon_capture_rule_test();
    general_facing_rule_test();
    apply_move_switches_side_test();
    uci_codec_round_trip_test();
    pikafish_process_adapter_test();
    search_prefers_winning_capture_test();
    time_budgeted_search_returns_playable_move_test();
    search_report_exposes_metadata_test();
    browser_session_bridge_test();
    return 0;
}
