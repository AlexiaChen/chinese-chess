cmake_minimum_required(VERSION 3.20)

project(chinese_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHINESE_CHESS_BUILD_TESTS "Build Chinese Chess tests" ON)

add_library(chinese_chess_core
    src/bridge/browser_session.cpp
    src/bridge/wasm_exports.cpp
    src/core/game.cpp
    src/engine/opening_book.cpp
    src/engine/pikafish_search.cpp
    src/engine/pikafish_nnue.cpp
    src/engine/pikafish_process.cpp
    src/engine/search.cpp
    src/engine/uci_codec.cpp
    third_party/pikafish/src/benchmark.cpp
    third_party/pikafish/src/bitboard.cpp
    third_party/pikafish/src/engine.cpp
    third_party/pikafish/src/memory.cpp
    third_party/pikafish/src/misc.cpp
    third_party/pikafish/src/movepick.cpp
    third_party/pikafish/src/movegen.cpp
    third_party/pikafish/src/position.cpp
    third_party/pikafish/src/evaluate.cpp
    third_party/pikafish/src/score.cpp
    third_party/pikafish/src/search.cpp
    third_party/pikafish/src/thread.cpp
    third_party/pikafish/src/timeman.cpp
    third_party/pikafish/src/tt.cpp
    third_party/pikafish/src/uci.cpp
    third_party/pikafish/src/ucioption.cpp
    third_party/pikafish/src/nnue/network.cpp
    third_party/pikafish/src/nnue/nnue_accumulator.cpp
    third_party/pikafish/src/nnue/nnue_misc.cpp
    third_party/pikafish/src/nnue/features/full_threats.cpp
    third_party/pikafish/src/nnue/features/half_ka_v2_hm.cpp
    third_party/pikafish/src/external/common/debug.cpp
    third_party/pikafish/src/external/common/entropy_common.cpp
    third_party/pikafish/src/external/common/error_private.cpp
    third_party/pikafish/src/external/common/fse_decompress.cpp
    third_party/pikafish/src/external/common/pool.cpp
    third_party/pikafish/src/external/common/threading.cpp
    third_party/pikafish/src/external/common/xxhash.cpp
    third_party/pikafish/src/external/common/zstd_common.cpp
    third_party/pikafish/src/external/decompress/huf_decompress.cpp
    third_party/pikafish/src/external/decompress/zstd_ddict.cpp
    third_party/pikafish/src/external/decompress/zstd_decompress.cpp
    third_party/pikafish/src/external/decompress/zstd_decompress_block.cpp
)

target_include_directories(chinese_chess_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(chinese_chess_core PUBLIC cxx_std_20)
target_compile_definitions(
    chinese_chess_core
    PRIVATE
        CHINESE_CHESS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NO_PREFETCH
        ZSTD_DISABLE_ASM
        DISABLE_SHARED_MEMORY
 )

add_executable(chinese_chess_cli
    src/apps/cli/main.cpp
)

target_link_libraries(chinese_chess_cli PRIVATE chinese_chess_core)

add_custom_target(
    pikafish_build
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_pikafish.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)

add_custom_target(
    web_bundle
    COMMAND npm --prefix "${CMAKE_CURRENT_SOURCE_DIR}/web" run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)

if(EMSCRIPTEN)
    target_compile_options(
        chinese_chess_core
        PRIVATE
            "$<$<CONFIG:Release>:-O3>"
            "$<$<CONFIG:Release>:-flto>"
    )

    add_executable(chinese_chess_wasm
        src/bridge/wasm_entry.cpp
    )

    target_link_libraries(chinese_chess_wasm PRIVATE chinese_chess_core)

    target_compile_options(
        chinese_chess_wasm
        PRIVATE
            "$<$<CONFIG:Release>:-O3>"
            "$<$<CONFIG:Release>:-flto>"
    )

    set_target_properties(chinese_chess_wasm PROPERTIES OUTPUT_NAME "chinese_chess_wasm")

    target_link_options(
        chinese_chess_wasm
        PRIVATE
            "$<$<CONFIG:Release>:-O3>"
            "$<$<CONFIG:Release>:-flto>"
            "$<$<CONFIG:Release>:-sASSERTIONS=0>"
            "$<$<CONFIG:Release>:-sSTACK_OVERFLOW_CHECK=0>"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sSTACK_SIZE=8388608"
            "-sENVIRONMENT=web,worker"
            "-sEXPORT_ES6=1"
            "-sEXPORTED_FUNCTIONS=['_malloc','_free','_chinese_chess_current_fen','_chinese_chess_legal_moves_from','_chinese_chess_current_position_status','_chinese_chess_apply_move','_chinese_chess_undo_last_move','_chinese_chess_undo_count','_chinese_chess_apply_ai_move','_chinese_chess_apply_ai_move_with_limits','_chinese_chess_apply_ai_move_with_report','_chinese_chess_search_ai_move_for_fen_with_report','_chinese_chess_reset']"
            "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','ccall','cwrap','lengthBytesUTF8','stringToUTF8']"
            "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pikafish/src/pikafish.nnue@/pikafish.nnue"
            "-sMODULARIZE=1"
            "-sNO_EXIT_RUNTIME=1"
        )
else()
    add_custom_target(
        wasm_build
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_wasm.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()

if(CHINESE_CHESS_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(chinese_chess_tests
        tests/game_tests.cpp
    )

    target_link_libraries(chinese_chess_tests PRIVATE chinese_chess_core)
    target_compile_definitions(
        chinese_chess_tests
        PRIVATE CHINESE_CHESS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(NAME chinese_chess_tests COMMAND chinese_chess_tests)
endif()
