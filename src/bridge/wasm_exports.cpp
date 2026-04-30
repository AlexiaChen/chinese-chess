#include <string>
#include <vector>

#include "bridge/browser_session.h"

namespace chinese_chess::bridge {
namespace {

BrowserSession& session() {
    static BrowserSession instance;
    return instance;
}

std::string& shared_buffer() {
    static std::string buffer;
    return buffer;
}

std::string to_json_array(const std::vector<std::string>& items) {
    std::string json = "[";
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json += ",";
        }
        json += "\"";
        json += items[index];
        json += "\"";
    }
    json += "]";
    return json;
}

std::string to_json_bool(bool value) {
    return value ? "true" : "false";
}

std::string to_json(const AiMoveReport& report) {
    std::string json = "{";
    json += "\"move\":\"" + report.move + "\",";
    json += "\"score\":" + std::to_string(report.score) + ",";
    json += "\"completedDepth\":" + std::to_string(report.completed_depth) + ",";
    json += "\"visitedNodes\":" + std::to_string(report.visited_nodes) + ",";
    json += "\"elapsedMs\":" + std::to_string(report.elapsed_ms) + ",";
    json += "\"principalVariation\":" + to_json_array(report.principal_variation) + ",";
    json += "\"timedOut\":" + to_json_bool(report.timed_out);
    json += "}";
    return json;
}

}  // namespace
}  // namespace chinese_chess::bridge

extern "C" {

const char* chinese_chess_current_fen() {
    auto& buffer = chinese_chess::bridge::shared_buffer();
    buffer = chinese_chess::bridge::session().current_fen();
    return buffer.c_str();
}

const char* chinese_chess_legal_moves_from(const char* square) {
    auto& buffer = chinese_chess::bridge::shared_buffer();
    buffer = chinese_chess::bridge::to_json_array(
        chinese_chess::bridge::session().legal_moves_from(square));
    return buffer.c_str();
}

int chinese_chess_apply_move(const char* move) {
    return chinese_chess::bridge::session().apply_move(move) ? 1 : 0;
}

int chinese_chess_undo_last_move() {
    return chinese_chess::bridge::session().undo_last_move() ? 1 : 0;
}

int chinese_chess_undo_count() {
    return static_cast<int>(chinese_chess::bridge::session().undo_count());
}

const char* chinese_chess_apply_ai_move(int depth) {
    auto& buffer = chinese_chess::bridge::shared_buffer();
    buffer = chinese_chess::bridge::session().apply_ai_move(depth);
    return buffer.c_str();
}

const char* chinese_chess_apply_ai_move_with_limits(int max_depth, int time_budget_ms) {
    auto& buffer = chinese_chess::bridge::shared_buffer();
    buffer = chinese_chess::bridge::session().apply_ai_move_with_limits(max_depth, time_budget_ms);
    return buffer.c_str();
}

const char* chinese_chess_apply_ai_move_with_report(int max_depth, int time_budget_ms) {
    auto& buffer = chinese_chess::bridge::shared_buffer();
    buffer = chinese_chess::bridge::to_json(
        chinese_chess::bridge::session().apply_ai_move_with_report(max_depth, time_budget_ms));
    return buffer.c_str();
}

void chinese_chess_reset() {
    chinese_chess::bridge::session().reset();
}

}
