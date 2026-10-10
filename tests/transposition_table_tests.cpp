#include "transposition_table.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace aggrochess;

namespace {

void testScoreConversion() {
    assert(scoreToTranspositionTable(125, 7) == 125);
    assert(scoreFromTranspositionTable(125, 7) == 125);

    const int winningMate = MateScore - 5;
    const int losingMate = -MateScore + 5;
    assert(scoreToTranspositionTable(winningMate, 3) == MateScore - 2);
    assert(scoreFromTranspositionTable(MateScore - 2, 3) == winningMate);
    assert(scoreToTranspositionTable(losingMate, 3) == -MateScore + 2);
    assert(scoreFromTranspositionTable(-MateScore + 2, 3) == losingMate);

    // Probing at a later ply makes the same stored mate farther away.
    assert(scoreFromTranspositionTable(MateScore - 2, 7) == MateScore - 9);
    assert(scoreFromTranspositionTable(-MateScore + 2, 7) == -MateScore + 9);
}

void testStoreAndProbe() {
    TranspositionTable table(1);
    const Move move(Square::E2, Square::E4, MoveType::PawnDoublePush);
    table.store(0x123456789ABCDEF0ULL, 12, 87, Bound::Lower, move, 4);

    const auto found = table.probe(0x123456789ABCDEF0ULL, 4);
    assert(found.has_value());
    assert(found->key == 0x123456789ABCDEF0ULL);
    assert(found->depth == 12);
    assert(found->score == 87);
    assert(found->bound == Bound::Lower);
    assert(found->bestMove == move);
    assert(found->generation == 0);
    assert(!table.probe(0x123456789ABCDEE0ULL, 4).has_value());

    table.store(0, 3, -7, Bound::Upper, Move{}, 0);
    const auto zeroKey = table.probe(0, 0);
    assert(zeroKey.has_value() && zeroKey->score == -7);

    table.store(42, 8, MateScore - 6, Bound::Exact, move, 5);
    const auto mateAtSamePly = table.probe(42, 5);
    const auto mateAtLaterPly = table.probe(42, 9);
    assert(mateAtSamePly->score == MateScore - 6);
    assert(mateAtLaterPly->score == MateScore - 10);
}

void testSameKeyReplacement() {
    TranspositionTable table(0);
    const ZobristKey key = 77;
    const Move deepMove(Square::G1, Square::F3, MoveType::Quiet);
    const Move shallowMove(Square::B1, Square::C3, MoveType::Quiet);

    table.store(key, 10, 40, Bound::Lower, deepMove, 0);
    table.store(key, 4, -20, Bound::Upper, shallowMove, 0);
    auto found = table.probe(key, 0);
    assert(found->depth == 10 && found->score == 40);
    assert(found->bestMove == deepMove);

    table.store(key, 4, 12, Bound::Exact, shallowMove, 0);
    found = table.probe(key, 0);
    assert(found->depth == 4 && found->score == 12);
    assert(found->bound == Bound::Exact);
    assert(found->bestMove == shallowMove);
}

void testClusterCollisionAndVerifiedKeys() {
    TranspositionTable table(0);
    const std::size_t stride = table.clusterCount();
    const ZobristKey base = 3;
    for (int index = 0; index < 4; ++index) {
        table.store(base + static_cast<ZobristKey>(index) * stride,
                    index + 1, index * 10, Bound::Upper, Move{}, 0);
    }
    for (int index = 0; index < 4; ++index) {
        const ZobristKey key = base + static_cast<ZobristKey>(index) * stride;
        const auto found = table.probe(key, 0);
        assert(found.has_value() && found->key == key);
    }

    const ZobristKey fifth = base + 4U * stride;
    table.store(fifth, 5, 50, Bound::Lower, Move{}, 0);
    assert(table.probe(fifth, 0).has_value());
    assert(!table.probe(base, 0).has_value());
    for (int index = 1; index < 4; ++index) {
        const ZobristKey key = base + static_cast<ZobristKey>(index) * stride;
        assert(table.probe(key, 0).has_value());
    }
}

void testGenerationAging() {
    TranspositionTable table(0);
    const std::size_t stride = table.clusterCount();
    const ZobristKey base = 9;
    for (int index = 0; index < 4; ++index) {
        table.store(base + static_cast<ZobristKey>(index) * stride,
                    30 + index, 0, Bound::Upper, Move{}, 0);
    }
    for (int generation = 0; generation < 4; ++generation) table.newSearch();

    // Refresh the first entry in the current generation, albeit at lower depth.
    table.store(base, 10, 1, Bound::Exact, Move{}, 0);
    const ZobristKey newcomer = base + 4U * stride;
    table.store(newcomer, 1, 2, Bound::Upper, Move{}, 0);
    assert(table.probe(base, 0).has_value());
    assert(table.probe(newcomer, 0).has_value());
    // The oldest shallow-quality entry is preferred over the refreshed entry.
    assert(!table.probe(base + stride, 0).has_value());
}

void testResizeClearAndValidation() {
    TranspositionTable table(1);
    assert(table.clusterCount() > 0);
    assert((table.clusterCount() & (table.clusterCount() - 1U)) == 0);
    assert(table.entryCount() == table.clusterCount() * 4U);
    assert(table.allocatedBytes() <= 1024U * 1024U);

    table.store(1, 1, 1, Bound::Exact, Move{}, 0);
    table.newSearch();
    assert(table.generation() == 1);
    table.clear();
    assert(table.generation() == 0);
    assert(!table.probe(1, 0).has_value());

    table.resize(2);
    assert(table.allocatedBytes() <= 2U * 1024U * 1024U);
    assert(table.allocatedBytes() >= 1024U * 1024U);

    bool threw = false;
    try { table.store(2, 1, 0, Bound::None, Move{}, 0); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false;
    try { table.store(2, 100000, 0, Bound::Exact, Move{}, 0); }
    catch (const std::out_of_range&) { threw = true; }
    assert(threw);
}

void testConcurrentAccessAndResize() {
    TranspositionTable table(1);
    constexpr int threadCount = 8;
    constexpr int iterations = 2000;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([&, thread] {
            while (!start.load(std::memory_order_acquire)) {}
            const ZobristKey key = 1000U + static_cast<ZobristKey>(thread);
            for (int iteration = 0; iteration < iterations; ++iteration) {
                const int depth = iteration % 64;
                table.store(key, depth, thread * 10, Bound::Exact,
                            Move(Square::A2, Square::A3, MoveType::Quiet), 0);
                const auto found = table.probe(key, 0);
                if (found.has_value()) assert(found->key == key);
            }
        });
    }
    threads.emplace_back([&] {
        while (!start.load(std::memory_order_acquire)) {}
        for (int iteration = 0; iteration < 20; ++iteration) {
            table.resize(iteration % 2 == 0 ? 1U : 2U);
        }
    });

    start.store(true, std::memory_order_release);
    for (std::thread& thread : threads) thread.join();

    table.store(9999, 5, 25, Bound::Exact, Move{}, 0);
    const auto found = table.probe(9999, 0);
    assert(found.has_value() && found->score == 25);
}

}  // namespace

int main() {
    testScoreConversion();
    testStoreAndProbe();
    testSameKeyReplacement();
    testClusterCollisionAndVerifiedKeys();
    testGenerationAging();
    testResizeClearAndValidation();
    testConcurrentAccessAndResize();
}
