#include "evaluation.h"

#include <cassert>
#include <stdexcept>

using namespace aggrochess;

namespace {

void testBalancedAndSideRelativeScores() {
    const Position start = Position::startPosition();
    const EvaluationBreakdown result = evaluateDetailed(start);
    assert(result.phase == 24);
    assert(result.whitePerspective == 0);
    assert(result.sideToMoveScore == 0);
    assert(evaluate(start) == 0);

    const Position whiteExtraRook = Position::fromFen(
        "4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const Position sameBlackToMove = Position::fromFen(
        "4k3/8/8/8/8/8/8/R3K3 b - - 0 1");
    assert(evaluateDetailed(whiteExtraRook).whitePerspective > 400);
    assert(evaluate(whiteExtraRook) == -evaluate(sameBlackToMove));
}

void testColorAndBoardSymmetry() {
    const Position whiteRook = Position::fromFen(
        "4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const Position blackRook = Position::fromFen(
        "r3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    const auto white = evaluateDetailed(whiteRook);
    const auto black = evaluateDetailed(blackRook);
    assert(white.whitePerspective == -black.whitePerspective);
    assert(white.sideToMoveScore == black.sideToMoveScore);
}

void testComplexSymmetry() {
    const Position original = Position::fromFen(
        "4k2r/2p2pp1/3b4/4P3/2N5/8/PP3PPP/R3K3 w - - 0 1");
    const Position mirrored = Position::fromFen(
        "3k3r/ppp3pp/8/5n2/3p4/4B3/1PP2P2/R2K4 b - - 0 1");
    const auto first = evaluateDetailed(original);
    const auto second = evaluateDetailed(mirrored);
    assert(first.whitePerspective == -second.whitePerspective);
    assert(first.sideToMoveScore == second.sideToMoveScore);
    assert(first.material.middlegame == -second.material.middlegame);
    assert(first.pawnStructure.endgame == -second.pawnStructure.endgame);
    assert(first.kingSafety.middlegame == -second.kingSafety.middlegame);
}

void testMaterialAndPhase() {
    const Position bareKings = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    const Position rook = Position::fromFen(
        "4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const auto bare = evaluateDetailed(bareKings);
    const auto withRook = evaluateDetailed(rook);
    assert(bare.phase == 0);
    assert(withRook.phase == 2);
    assert(withRook.material.middlegame == 500);
    assert(withRook.material.endgame == 510);
}

void testPositionalTerms() {
    const Position centerKnight = Position::fromFen(
        "4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
    const Position rimKnight = Position::fromFen(
        "4k3/8/8/8/N7/8/8/4K3 w - - 0 1");
    assert(evaluateDetailed(centerKnight).pieceSquare.middlegame >
           evaluateDetailed(rimKnight).pieceSquare.middlegame);
    assert(evaluateDetailed(centerKnight).mobility.middlegame >
           evaluateDetailed(rimKnight).mobility.middlegame);

    const Position bishopPair = Position::fromFen(
        "4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1");
    assert(evaluateDetailed(bishopPair).bishopPair.middlegame == 28);

    const Position openRook = Position::fromFen(
        "4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const Position closedRook = Position::fromFen(
        "4k3/8/8/8/8/8/P7/R3K3 w - - 0 1");
    assert(evaluateDetailed(openRook).rookActivity.middlegame >
           evaluateDetailed(closedRook).rookActivity.middlegame);
}

void testPawnStructureAndKingSafety() {
    const Position passed = Position::fromFen(
        "4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    const Position opposed = Position::fromFen(
        "4k3/4p3/8/4P3/8/8/8/4K3 w - - 0 1");
    assert(evaluateDetailed(passed).pawnStructure.endgame >
           evaluateDetailed(opposed).pawnStructure.endgame);

    const Position shielded = Position::fromFen(
        "4k3/8/8/8/8/8/3PPP2/4K3 w - - 0 1");
    const Position exposed = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    assert(evaluateDetailed(shielded).kingSafety.middlegame >
           evaluateDetailed(exposed).kingSafety.middlegame);
}

void testInvalidPositionRejected() {
    const Position noKings = Position::fromFen("8/8/8/8/8/8/8/8 w - - 0 1");
    bool threw = false;
    try { (void)evaluate(noKings); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

}  // namespace

int main() {
    testBalancedAndSideRelativeScores();
    testColorAndBoardSymmetry();
    testComplexSymmetry();
    testMaterialAndPhase();
    testPositionalTerms();
    testPawnStructureAndKingSafety();
    testInvalidPositionRejected();
}
