#include "attacks.h"

#include <cassert>
#include <initializer_list>

using namespace aggrochess;

namespace {

Bitboard mask(std::initializer_list<Square> squares) {
    Bitboard result = 0;
    for (const Square square : squares) setSquare(result, square);
    return result;
}

void testPawnAttacks() {
    assert(pawnAttacks(Color::White, Square::A2) == mask({Square::B3}));
    assert(pawnAttacks(Color::Black, Square::H7) == mask({Square::G6}));
    assert(pawnAttacks(Color::White, Square::D4) ==
           mask({Square::C5, Square::E5}));
    assert(pawnAttacks(Color::Black, Square::D4) ==
           mask({Square::C3, Square::E3}));
}

void testLeaperAttacks() {
    assert(knightAttacks(Square::D4) ==
           mask({Square::B3, Square::B5, Square::C2, Square::C6,
                 Square::E2, Square::E6, Square::F3, Square::F5}));
    assert(knightAttacks(Square::A1) ==
           mask({Square::B3, Square::C2}));
    assert(kingAttacks(Square::A1) ==
           mask({Square::A2, Square::B1, Square::B2}));
    assert(countBits(kingAttacks(Square::D4)) == 8);
}

void testSlidingAttacksAndBlockers() {
    const Bitboard bishopBlockers = mask({Square::F6, Square::B2});
    assert(bishopAttacks(Square::D4, bishopBlockers) ==
           mask({Square::E5, Square::F6,
                 Square::C5, Square::B6, Square::A7,
                 Square::E3, Square::F2, Square::G1,
                 Square::C3, Square::B2}));

    const Bitboard rookBlockers = mask({Square::D6, Square::B4});
    assert(rookAttacks(Square::D4, rookBlockers) ==
           mask({Square::D5, Square::D6,
                 Square::D3, Square::D2, Square::D1,
                 Square::E4, Square::F4, Square::G4, Square::H4,
                 Square::C4, Square::B4}));
    assert(queenAttacks(Square::D4, bishopBlockers | rookBlockers) ==
           (bishopAttacks(Square::D4, bishopBlockers | rookBlockers) |
            rookAttacks(Square::D4, bishopBlockers | rookBlockers)));
}

void testPositionAttackDetection() {
    const Position position = Position::fromFen(
        "4k3/8/2n5/8/3P4/8/8/4K3 w - - 0 1");
    assert(isSquareAttacked(position, Square::D4, Color::Black));
    assert(!isSquareAttacked(position, Square::E1, Color::Black));
    assert(!isInCheck(position, Color::White));

    const Position rookCheck = Position::fromFen(
        "4r1k1/8/8/8/8/8/8/4K3 w - - 0 1");
    assert(isInCheck(rookCheck, Color::White));

    const Position blockedCheck = Position::fromFen(
        "4r1k1/8/8/8/4P3/8/8/4K3 w - - 0 1");
    assert(!isInCheck(blockedCheck, Color::White));
}

}  // namespace

int main() {
    testPawnAttacks();
    testLeaperAttacks();
    testSlidingAttacksAndBlockers();
    testPositionAttackDetection();
}
