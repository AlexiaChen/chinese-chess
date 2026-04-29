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

void chinese_chess_reset() {
    chinese_chess::bridge::session().reset();
}

}
