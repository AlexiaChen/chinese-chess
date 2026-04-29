#include "core/game.h"
#include "engine/pikafish_process.h"
#include "engine/uci_codec.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage() {
    std::cout
        << "Usage:\n"
        << "  chinese_chess_cli\n"
        << "  chinese_chess_cli --engine-cmd <command> [--depth <n>] [--fen <fen>]\n";
}

}  // namespace

int main(int argc, char** argv) {
    chinese_chess::GameState game = chinese_chess::GameState::initial();
    std::string engine_command;
    int depth = 1;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--engine-cmd") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--engine-cmd requires a value");
            }
            engine_command = argv[++index];
        } else if (argument == "--depth") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--depth requires a value");
            }
            depth = std::stoi(argv[++index]);
        } else if (argument == "--fen") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--fen requires a value");
            }
            game = chinese_chess::GameState::from_fen(argv[++index]);
        } else if (argument == "--help") {
            print_usage();
            return EXIT_SUCCESS;
        } else {
            throw std::invalid_argument("Unknown argument: " + argument);
        }
    }

    if (engine_command.empty()) {
        std::cout << game.debug_string() << '\n';
        std::cout << "fen=" << game.to_fen() << '\n';
        return EXIT_SUCCESS;
    }

    chinese_chess::engine::PikafishProcess engine(engine_command);
    const chinese_chess::Move best_move = engine.find_best_move(game, depth);
    std::cout << "bestmove=" << chinese_chess::engine::to_uci_move(best_move) << '\n';
    return 0;
}
