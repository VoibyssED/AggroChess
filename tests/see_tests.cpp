#include "see.h"

#include <cassert>
#include <stdexcept>

using namespace aggrochess;

namespace {

void testPieceValues() {
    static_assert(pieceValue(PieceType::Pawn) == 100);
    static_assert(pieceValue(PieceType::Knight) == 320);
    static_assert(pieceValue(PieceType::Bishop) == 330);
    static_assert(pieceValue(PieceType::Rook) == 500);
    static_assert(pieceValue(PieceType::Queen) == 900);
}

void testWinningEqualAndLosingCaptures() {
    const Position winning = Position::fromFen(
        "4k3/8/8/4q3/3P4/8/8/4K3 w - - 0 1");
    assert(staticExchangeEvaluation(
               winning, Move(Square::D4, Square::E5, MoveType::Capture)) == 900);

    const Position equal = Position::fromFen(
        "4k3/8/5p2/4p3/3P4/8/8/4K3 w - - 0 1");
    assert(staticExchangeEvaluation(
               equal, Move(Square::D4, Square::E5, MoveType::Capture)) == 0);

    const Position losing = Position::fromFen(
        "3rk3/8/8/3p4/8/8/8/3QK3 w - - 0 1");
    assert(staticExchangeEvaluation(
               losing, Move(Square::D1, Square::D5, MoveType::Capture)) == -800);
}

void testXrayAndPinnedDefender() {
    const Position xray = Position::fromFen(
        "3rk3/8/8/3p4/8/3R4/8/3QK3 w - - 0 1");
    assert(staticExchangeEvaluation(
               xray, Move(Square::D3, Square::D5, MoveType::Capture)) == 100);

    const Position pinnedKnight = Position::fromFen(
        "4k3/4n3/8/3p4/2P5/8/8/K3R3 w - - 0 1");
    assert(staticExchangeEvaluation(
               pinnedKnight,
               Move(Square::C4, Square::D5, MoveType::Capture)) == 100);
}

void testKingCaptureLegality() {
    const Position defendedTarget = Position::fromFen(
        "8/8/4k3/3p4/2P1P3/8/8/4K3 w - - 0 1");
    // The black king attacks d5, but cannot recapture there because e4
    // protects the square.
    assert(staticExchangeEvaluation(
               defendedTarget,
               Move(Square::C4, Square::D5, MoveType::Capture)) == 100);
}

void testEnPassantAndPromotions() {
    const Position enPassant = Position::fromFen(
        "3rk3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    assert(staticExchangeEvaluation(
               enPassant,
               Move(Square::E5, Square::D6, MoveType::EnPassant)) == 0);

    const Position quietPromotion = Position::fromFen(
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    assert(staticExchangeEvaluation(
               quietPromotion,
               Move(Square::A7, Square::A8, MoveType::Promotion,
                    PieceType::Queen)) == 800);
    assert(staticExchangeEvaluation(
               quietPromotion,
               Move(Square::A7, Square::A8, MoveType::Promotion,
                    PieceType::Knight)) == 220);

    const Position capturePromotion = Position::fromFen(
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    assert(staticExchangeEvaluation(
               capturePromotion,
               Move(Square::A7, Square::B8, MoveType::PromotionCapture,
                    PieceType::Queen)) == 1300);
    assert(staticExchangeEvaluation(
               capturePromotion,
               Move(Square::A7, Square::B8, MoveType::PromotionCapture,
                    PieceType::Knight)) == 720);
}

void testQuietHangingMove() {
    const Position position = Position::fromFen(
        "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    assert(staticExchangeEvaluation(
               position, Move(Square::D1, Square::D5, MoveType::Quiet)) == -900);
}

void testCompetingRecaptureChoices() {
    // Choosing captures solely by nominal attacker value gives the wrong result
    // here; SEE must account for the complete legal continuation of each choice.
    const Position position = Position::fromFen(
        "1r1n4/pRBk4/4pp2/P1pp1rp1/1nP1PP2/P2P3R/1QN1N3/5K1B b - f3 0 36");
    assert(staticExchangeEvaluation(
               position, Move(Square::F5, Square::F4, MoveType::Capture)) == -400);
}

void testInvalidAndKingExposingMoves() {
    const Position pinned = Position::fromFen(
        "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    bool threw = false;
    try {
        (void)staticExchangeEvaluation(
            pinned, Move(Square::E2, Square::D2, MoveType::Quiet));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try { (void)staticExchangeEvaluation(pinned, Move{}); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

}  // namespace

int main() {
    testPieceValues();
    testWinningEqualAndLosingCaptures();
    testXrayAndPinnedDefender();
    testKingCaptureLegality();
    testEnPassantAndPromotions();
    testQuietHangingMove();
    testCompetingRecaptureChoices();
    testInvalidAndKingExposingMoves();
}
