#include "engine/pikafish_nnue.h"

#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include "../../third_party/pikafish/src/bitboard.h"
#include "../../third_party/pikafish/src/evaluate.h"
#include "../../third_party/pikafish/src/nnue/network.h"
#include "../../third_party/pikafish/src/nnue/nnue_accumulator.h"
#include "../../third_party/pikafish/src/nnue/nnue_misc.h"
#include "../../third_party/pikafish/src/position.h"
#include "../../third_party/pikafish/src/tt.h"
#include "../../third_party/pikafish/src/uci.h"

namespace chinese_chess::engine {

namespace {

constexpr char kPikafishEvalFileDefaultName[] = "pikafish.nnue";

std::string resolve_nnue_path() {
#ifdef __EMSCRIPTEN__
    return "/pikafish.nnue";
#else
    return std::string(CHINESE_CHESS_SOURCE_DIR) + "/third_party/pikafish/src/pikafish.nnue";
#endif
}

class PikafishNnueModel {
   public:
    PikafishNnueModel() :
        networks_(Stockfish::Eval::NNUE::EvalFile {kPikafishEvalFileDefaultName, "None", ""}) {
        static std::once_flag stockfish_init_once;
        std::call_once(stockfish_init_once, []() {
            Stockfish::Bitboards::init();
            Stockfish::Position::init();
        });

        const std::string path = resolve_nnue_path();
        networks_.big.load("", path);
        if (networks_.big.get_content_hash() == 0) {
            throw std::runtime_error("Failed to load Pikafish NNUE network from " + path);
        }

        accumulators_ = std::make_unique<Stockfish::Eval::NNUE::AccumulatorStack>();
        caches_ = std::make_unique<Stockfish::Eval::NNUE::AccumulatorCaches>(networks_);
        position_ = std::make_unique<Stockfish::Position>();
        state_info_ = std::make_unique<Stockfish::StateInfo>();
    }

    std::optional<int> try_evaluate(const GameState& state) const {
        const std::string fen = state.to_fen();
        std::lock_guard<std::mutex> lock(workspace_mutex_);
        *state_info_ = Stockfish::StateInfo {};
        if (const auto error = position_->set(fen, state_info_.get()); error.has_value()) {
            return std::nullopt;
        }
        accumulators_->reset();
        return Stockfish::Eval::evaluate(
            networks_,
            *position_,
            *accumulators_,
            *caches_,
            Stockfish::VALUE_ZERO);
    }

    private:
    Stockfish::Eval::NNUE::Networks networks_;
    mutable std::mutex workspace_mutex_;
    mutable std::unique_ptr<Stockfish::Eval::NNUE::AccumulatorStack> accumulators_;
    mutable std::unique_ptr<Stockfish::Eval::NNUE::AccumulatorCaches> caches_;
    mutable std::unique_ptr<Stockfish::Position> position_;
    mutable std::unique_ptr<Stockfish::StateInfo> state_info_;
};

const PikafishNnueModel& model() {
    static const PikafishNnueModel instance;
    return instance;
}

}  // namespace

std::optional<int> try_evaluate_position_with_pikafish_nnue(const GameState& state) {
    return model().try_evaluate(state);
}

}  // namespace chinese_chess::engine

namespace Stockfish {

}  // namespace Stockfish
