#include "board.h"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace aggrochess;

namespace {

void expectInvalidAndUnchanged(Position& position, const Move& move) {
    const std::string before = position.toFen();
    Undo undo;
    bool threw = false;
    try {
        makeMove(position, move, undo);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(position.toFen() == before);
    assert(position.isValid());
}

void roundTrip(Position& position, const Move& move, const std::string& after) {
    const std::string before = position.toFen();
    Undo undo;
    makeMove(position, move, undo);
    assert(position.isValid());
    assert(position.toFen() == after);
    unmakeMove(position, undo);
    assert(position.isValid());
    assert(position.toFen() == before);
}

void testQuietAndDoublePush() {
    Position position = Position::startPosition();
    const std::string before = position.toFen();
    Undo undo;
    makeMove(position, Move(Square::E2, Square::E4, MoveType::PawnDoublePush), undo);
    assert(position.toFen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    Undo replyUndo;
    makeMove(position, Move(Square::B8, Square::A6, MoveType::Quiet), replyUndo);
    assert(position.toFen() == "r1bqkbnr/pppppppp/n7/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2");
    unmakeMove(position, replyUndo);
    unmakeMove(position, undo);
    assert(position.toFen() == before);
    roundTrip(position, Move(Square::G1, Square::F3, MoveType::Quiet),
              "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
}

void testCaptureAndCounters() {
    Position position = Position::fromFen("4k3/8/8/8/3p4/8/4N3/4K3 w - - 7 12");
    roundTrip(position, Move(Square::E2, Square::D4, MoveType::Capture),
              "4k3/8/8/8/3N4/8/8/4K3 b - - 0 12");
    position = Position::fromFen("4k3/8/8/8/8/8/2N5/4K3 w - - 7 12");
    roundTrip(position, Move(Square::C2, Square::B4, MoveType::Quiet),
              "4k3/8/8/8/1N6/8/8/4K3 b - - 8 12");
    position = Position::fromFen("4k3/2n5/8/8/8/8/8/4K3 b - - 7 12");
    roundTrip(position, Move(Square::C7, Square::B5, MoveType::Quiet),
              "4k3/8/8/1n6/8/8/8/4K3 w - - 8 13");
}

void testEnPassantBothColors() {
    Position white = Position::fromFen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 4 20");
    roundTrip(white, Move(Square::E5, Square::D6, MoveType::EnPassant),
              "4k3/8/3P4/8/8/8/8/4K3 b - - 0 20");
    Position black = Position::fromFen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 4 20");
    roundTrip(black, Move(Square::D4, Square::E3, MoveType::EnPassant),
              "4k3/8/8/8/8/4p3/8/4K3 w - - 0 21");
}

void testCastling() {
    Position whiteKingSide = Position::fromFen("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
    roundTrip(whiteKingSide, Move(Square::E1, Square::G1, MoveType::Castle),
              "4k3/8/8/8/8/8/8/5RK1 b - - 1 1");
    Position whiteQueenSide = Position::fromFen("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
    roundTrip(whiteQueenSide, Move(Square::E1, Square::C1, MoveType::Castle),
              "4k3/8/8/8/8/8/8/2KR4 b - - 1 1");
    Position blackKingSide = Position::fromFen("4k2r/8/8/8/8/8/8/4K3 b k - 0 1");
    roundTrip(blackKingSide, Move(Square::E8, Square::G8, MoveType::Castle),
              "5rk1/8/8/8/8/8/8/4K3 w - - 1 2");
    Position blackQueenSide = Position::fromFen("r3k3/8/8/8/8/8/8/4K3 b q - 0 1");
    roundTrip(blackQueenSide, Move(Square::E8, Square::C8, MoveType::Castle),
              "2kr4/8/8/8/8/8/8/4K3 w - - 1 2");
}

void testCastlingRightsLoss() {
    Position position = Position::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    Undo undo;
    makeMove(position, Move(Square::H1, Square::H2, MoveType::Quiet), undo);
    assert((position.castlingRights() & WhiteKingSide) == 0);
    unmakeMove(position, undo);
    makeMove(position, Move(Square::A1, Square::A2, MoveType::Quiet), undo);
    assert((position.castlingRights() & WhiteQueenSide) == 0);
    unmakeMove(position, undo);
    makeMove(position, Move(Square::E1, Square::F1, MoveType::Quiet), undo);
    assert((position.castlingRights() & (WhiteKingSide | WhiteQueenSide)) == 0);
    unmakeMove(position, undo);
    position = Position::fromFen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    makeMove(position, Move(Square::H8, Square::H7, MoveType::Quiet), undo);
    assert((position.castlingRights() & BlackKingSide) == 0);
    unmakeMove(position, undo);
    makeMove(position, Move(Square::A8, Square::A7, MoveType::Quiet), undo);
    assert((position.castlingRights() & BlackQueenSide) == 0);
    unmakeMove(position, undo);
    makeMove(position, Move(Square::E8, Square::F8, MoveType::Quiet), undo);
    assert((position.castlingRights() & (BlackKingSide | BlackQueenSide)) == 0);
    unmakeMove(position, undo);
    position = Position::fromFen("r3k2r/8/8/8/8/8/8/4K2R w KQkq - 0 1");
    makeMove(position, Move(Square::H1, Square::A8, MoveType::Capture), undo);
    assert((position.castlingRights() & BlackQueenSide) == 0);
    assert((position.castlingRights() & BlackKingSide) != 0);
    unmakeMove(position, undo);
}

void testPromotions() {
    const PieceType promotions[] = {PieceType::Knight, PieceType::Bishop,
                                    PieceType::Rook, PieceType::Queen};
    const char whiteSymbols[] = {'N', 'B', 'R', 'Q'};
    const char blackSymbols[] = {'n', 'b', 'r', 'q'};
    for (const auto promotion : promotions) {
        const int promotionIndex = static_cast<int>(promotion) -
                                    static_cast<int>(PieceType::Knight);
        Position white = Position::fromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
        roundTrip(white, Move(Square::A7, Square::A8, MoveType::Promotion, promotion),
                  std::string(1, whiteSymbols[promotionIndex]) +
                  "3k3/8/8/8/8/8/8/4K3 b - - 0 1");
        Position black = Position::fromFen("4k3/8/8/8/8/8/p7/4K3 b - - 0 1");
        roundTrip(black, Move(Square::A2, Square::A1, MoveType::Promotion, promotion),
                  "4k3/8/8/8/8/8/8/" +
                  std::string(1, blackSymbols[promotionIndex]) +
                  "3K3 w - - 0 2");
    }
    Position capture = Position::fromFen("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    roundTrip(capture, Move(Square::A7, Square::B8, MoveType::PromotionCapture,
                            PieceType::Queen),
              "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1");
}

void testRestorationSequence() {
    Position position = Position::fromFen("4k2r/P6p/8/3pP3/8/8/6p1/R3K2R w KQkq d6 9 30");
    const std::string before = position.toFen();
    Undo first;
    makeMove(position, Move(Square::E5, Square::D6, MoveType::EnPassant), first);
    Undo second;
    makeMove(position, Move(Square::H8, Square::G8, MoveType::Quiet), second);
    Undo third;
    makeMove(position, Move(Square::A7, Square::A8, MoveType::Promotion, PieceType::Queen), third);
    unmakeMove(position, third);
    unmakeMove(position, second);
    unmakeMove(position, first);
    assert(position.toFen() == before);
    assert(position.isValid());
}

void testInvalidMovesDoNotMutate() {
    Position empty = Position::fromFen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    expectInvalidAndUnchanged(empty, Move(Square::A1, Square::A2, MoveType::Quiet));

    Position enemyOnDestination = Position::fromFen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    expectInvalidAndUnchanged(enemyOnDestination,
                              Move(Square::A1, Square::E8, MoveType::Quiet));

    Position noCapture = Position::fromFen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    expectInvalidAndUnchanged(noCapture,
                              Move(Square::E1, Square::E2, MoveType::Capture));

    Position invalidPromotionPiece = Position::fromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    expectInvalidAndUnchanged(invalidPromotionPiece,
                              Move(Square::A7, Square::A8, MoveType::Promotion,
                                   PieceType::King));

    Position nonPawnPromotion = Position::fromFen("4k3/8/8/8/8/8/8/4K2R w - - 0 1");
    expectInvalidAndUnchanged(nonPawnPromotion,
                              Move(Square::H1, Square::H8, MoveType::Promotion,
                                   PieceType::Queen));

    Position noRook = Position::fromFen("4k3/8/8/8/8/8/8/4K3 w K - 0 1");
    expectInvalidAndUnchanged(noRook,
                              Move(Square::E1, Square::G1, MoveType::Castle));

    Position occupiedEnPassantDestination =
        Position::fromFen("4k3/8/3n4/3pP3/8/8/8/4K3 w - d6 0 1");
    expectInvalidAndUnchanged(occupiedEnPassantDestination,
                              Move(Square::E5, Square::D6, MoveType::EnPassant));
}

}

int main() {
    testQuietAndDoublePush();
    testCaptureAndCounters();
    testEnPassantBothColors();
    testCastling();
    testCastlingRightsLoss();
    testPromotions();
    testRestorationSequence();
    testInvalidMovesDoNotMutate();
}
