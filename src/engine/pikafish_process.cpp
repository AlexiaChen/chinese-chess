#include "engine/pikafish_process.h"

#include <cstring>
#include <stdexcept>
#include <string_view>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/uci_codec.h"

namespace chinese_chess::engine {

namespace {

constexpr auto kHandshakeTimeout = std::chrono::seconds(2);
constexpr auto kSearchTimeout = std::chrono::seconds(5);

std::string trim_newline(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

}  // namespace

PikafishProcess::PikafishProcess(std::string command) :
    command_(std::move(command)) {}

PikafishProcess::~PikafishProcess() {
    stop();
}

void PikafishProcess::start() {
    if (running()) {
        return;
    }

    int stdin_pipe[2] {};
    int stdout_pipe[2] {};
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        throw std::runtime_error("Failed to create process pipes");
    }

    const pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to fork Pikafish process");
    }

    if (pid == 0) {
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        execl("/bin/sh", "sh", "-c", command_.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    child_pid_ = pid;
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    child_in_fd_ = stdin_pipe[1];
    child_out_fd_ = stdout_pipe[0];
    read_buffer_.clear();

    send_command("uci");
    for (;;) {
        const std::string line = read_line(kHandshakeTimeout);
        if (line.rfind("id name ", 0) == 0) {
            engine_name_ = line.substr(std::strlen("id name "));
        }
        if (line == "uciok") {
            break;
        }
    }

    send_command("isready");
    for (;;) {
        if (read_line(kHandshakeTimeout) == "readyok") {
            break;
        }
    }
}

void PikafishProcess::stop() {
    if (!running()) {
        return;
    }

    send_command("quit");

    close(child_in_fd_);
    close(child_out_fd_);
    child_in_fd_ = -1;
    child_out_fd_ = -1;
    read_buffer_.clear();

    int status = 0;
    waitpid(child_pid_, &status, 0);
    child_pid_ = -1;
}

bool PikafishProcess::running() const {
    return child_pid_ > 0 && child_in_fd_ >= 0 && child_out_fd_ >= 0;
}

const std::string& PikafishProcess::engine_name() const {
    return engine_name_;
}

Move PikafishProcess::find_best_move(const GameState& state, int depth) {
    if (depth <= 0) {
        throw std::invalid_argument("Depth must be positive");
    }

    start();

    send_command("ucinewgame");
    send_command("isready");
    while (read_line(kHandshakeTimeout) != "readyok") {
    }

    send_command("position fen " + state.to_fen());
    send_command("go depth " + std::to_string(depth));

    for (;;) {
        const std::string line = read_line(kSearchTimeout);
        if (line.rfind("bestmove ", 0) != 0) {
            continue;
        }

        const std::string_view payload(line);
        const auto first_space = payload.find(' ');
        const auto second_space = payload.find(' ', first_space + 1);
        const std::string_view move_text = second_space == std::string_view::npos
            ? payload.substr(first_space + 1)
            : payload.substr(first_space + 1, second_space - first_space - 1);
        return from_uci_move(move_text);
    }
}

void PikafishProcess::send_command(std::string_view command) {
    if (!running()) {
        throw std::runtime_error("Pikafish process is not running");
    }

    std::string payload(command);
    payload.push_back('\n');

    ssize_t written = 0;
    while (written < static_cast<ssize_t>(payload.size())) {
        const ssize_t result = write(
            child_in_fd_,
            payload.data() + written,
            payload.size() - static_cast<std::size_t>(written));
        if (result <= 0) {
            throw std::runtime_error("Failed to write to Pikafish process");
        }
        written += result;
    }
}

std::string PikafishProcess::read_line(std::chrono::milliseconds timeout) {
    if (!running()) {
        throw std::runtime_error("Pikafish process is not running");
    }

    const auto buffered_newline = read_buffer_.find('\n');
    if (buffered_newline != std::string::npos) {
        std::string line = read_buffer_.substr(0, buffered_newline + 1);
        read_buffer_.erase(0, buffered_newline + 1);
        return trim_newline(std::move(line));
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(child_out_fd_, &read_set);

    timeval tv {};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    const int ready = select(child_out_fd_ + 1, &read_set, nullptr, nullptr, &tv);
    if (ready < 0) {
        throw std::runtime_error("Failed to wait for Pikafish output");
    }
    if (ready == 0) {
        throw std::runtime_error("Timed out waiting for Pikafish output");
    }

    char buffer[4096] {};
    const ssize_t bytes_read = read(child_out_fd_, buffer, sizeof(buffer));
    if (bytes_read <= 0) {
        throw std::runtime_error("Pikafish process closed its output");
    }

    read_buffer_.append(buffer, static_cast<std::size_t>(bytes_read));
    const auto newline = read_buffer_.find('\n');
    if (newline == std::string::npos) {
        return read_line(timeout);
    }

    std::string line = read_buffer_.substr(0, newline + 1);
    read_buffer_.erase(0, newline + 1);
    return trim_newline(std::move(line));
}

}  // namespace chinese_chess::engine
