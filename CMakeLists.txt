cmake_minimum_required(VERSION 3.20)

project(AggroChess VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

option(AGGROCHESS_ENABLE_SANITIZERS
       "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(aggrochess_warnings INTERFACE)
if(MSVC)
    target_compile_options(aggrochess_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(aggrochess_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

add_library(aggrochess_options INTERFACE)
target_compile_features(aggrochess_options INTERFACE cxx_std_20)

if(AGGROCHESS_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(aggrochess_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(aggrochess_options INTERFACE
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR
            "AGGROCHESS_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

add_library(aggrochess_core STATIC
    src/attacks.cpp
    src/board.cpp
    src/engine.cpp
    src/evaluation.cpp
    src/game_state.cpp
    src/movegen.cpp
    src/see.cpp
    src/transposition_table.cpp
    src/zobrist.cpp
)
add_library(AggroChess::core ALIAS aggrochess_core)

target_include_directories(aggrochess_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(aggrochess_core
    PUBLIC
        aggrochess_options
        Threads::Threads
    PRIVATE
        aggrochess_warnings
)

add_executable(aggrochess src/main.cpp)
set_target_properties(aggrochess PROPERTIES OUTPUT_NAME AggroChess)
target_link_libraries(aggrochess
    PRIVATE
        AggroChess::core
        aggrochess_options
        aggrochess_warnings
)

if(BUILD_TESTING)
    function(aggrochess_add_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target}
            PRIVATE
                AggroChess::core
                aggrochess_options
                aggrochess_warnings
        )

        # The current tests use assert(). Keep those checks active in every
        # configuration, including Release builds that normally define NDEBUG.
        if(MSVC)
            target_compile_options(${target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -UNDEBUG)
        endif()

        add_test(NAME ${target} COMMAND ${target})
    endfunction()

    aggrochess_add_test(attack_tests tests/attack_tests.cpp)
    aggrochess_add_test(board_tests tests/board_tests.cpp)
    aggrochess_add_test(engine_tests tests/engine_tests.cpp)
    aggrochess_add_test(evaluation_tests tests/evaluation_tests.cpp)
    aggrochess_add_test(game_state_tests tests/game_state_tests.cpp)
    aggrochess_add_test(move_notation_tests tests/move_notation_tests.cpp)
    aggrochess_add_test(move_tests tests/move_tests.cpp)
    aggrochess_add_test(movegen_tests tests/movegen_tests.cpp)
    aggrochess_add_test(see_tests tests/see_tests.cpp)
    aggrochess_add_test(transposition_table_tests tests/transposition_table_tests.cpp)
    aggrochess_add_test(zobrist_tests tests/zobrist_tests.cpp)
endif()
