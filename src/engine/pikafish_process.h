#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "core/game.h"

namespace chinese_chess::engine {

class PikafishProcess {
public:
    explicit PikafishProcess(std::string command);
    ~PikafishProcess();

    PikafishProcess(const PikafishProcess&) = delete;
    PikafishProcess& operator=(const PikafishProcess&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool running() const;
    [[nodiscard]] const std::string& engine_name() const;
    [[nodiscard]] Move find_best_move(const GameState& state, int depth);

private:
    std::string command_;
    std::string engine_name_;
    int child_pid_ {-1};
    int child_in_fd_ {-1};
    int child_out_fd_ {-1};
    std::string read_buffer_;

    void send_command(std::string_view command);
    [[nodiscard]] std::string read_line(std::chrono::milliseconds timeout);
};

}  // namespace chinese_chess::engine
