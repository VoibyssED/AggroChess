#include "board.h"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace aggrochess;

namespace {
void invalidFen(const std::string& fen) {
    bool threw = false;
    try { (void)Position::fromFen(fen); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testHelpers() {
    Bitboard board = 0;
    setSquare(board, Square::A1);
    setSquare(board, Square::H8);
    assert(testSquare(board, Square::A1) && testSquare(board, Square::H8));
    assert(countBits(board) == 2);
    assert(popLeastSignificantBit(board) == Square::A1);
    clearSquare(board, Square::H8);
    assert(board == 0);
}

void testStart() {
    const auto position = Position::startPosition();
    assert(position.toFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    assert(countBits(position.pieces(Color::White, PieceType::Pawn)) == 8);
    assert(countBits(position.pieces(Color::White, PieceType::Knight)) == 2);
    assert(countBits(position.pieces(Color::White, PieceType::Bishop)) == 2);
    assert(countBits(position.pieces(Color::White, PieceType::Rook)) == 2);
    assert(countBits(position.pieces(Color::White, PieceType::Queen)) == 1);
    assert(countBits(position.pieces(Color::White, PieceType::King)) == 1);
    assert(countBits(position.pieces(Color::Black, PieceType::Pawn)) == 8);
    assert(countBits(position.pieces(Color::Black, PieceType::Knight)) == 2);
    assert(countBits(position.pieces(Color::Black, PieceType::Bishop)) == 2);
    assert(countBits(position.pieces(Color::Black, PieceType::Rook)) == 2);
    assert(countBits(position.pieces(Color::Black, PieceType::Queen)) == 1);
    assert(countBits(position.pieces(Color::Black, PieceType::King)) == 1);
    assert(countBits(position.pieces(Color::Black)) == 16);
    assert(countBits(position.allOccupancy()) == 32);
    assert(position.occupancy(Color::White) == position.pieces(Color::White));
    assert(position.occupancy(Color::Black) == position.pieces(Color::Black));
    assert(position.allOccupancy() == (position.pieces(PieceType::Pawn) |
        position.pieces(PieceType::Knight) | position.pieces(PieceType::Bishop) |
        position.pieces(PieceType::Rook) | position.pieces(PieceType::Queen) |
        position.pieces(PieceType::King)));
}

void testFenState() {
    const std::string fen = "8/3k4/8/4p3/8/2N5/8/4K3 b q e3 17 42";
    const auto position = Position::fromFen(fen);
    assert(position.toFen() == fen);
    assert(position.sideToMove() == Color::Black);
    assert(position.castlingRights() == BlackQueenSide);
    assert(position.hasEnPassantSquare() && position.enPassantSquare() == Square::E3);
    assert(position.halfmoveClock() == 17 && position.fullmoveNumber() == 42);
}

void testEmptyAndPieceTypes() {
    const auto empty = Position::fromFen("8/8/8/8/8/8/8/8 w - - 0 1");
    assert(empty.allOccupancy() == 0);
    const auto position = Position::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 99 100");
    for (int i = 0; i < static_cast<int>(PieceType::Count); ++i)
        assert(position.pieces(static_cast<PieceType>(i)) != 0);
    assert(position.toFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 99 100");
}

void testMalformedFen() {
    invalidFen("8/8/8/8/8/8/8 w - - 0 1");
    invalidFen("8/8/8/8/8/8/8/8 x - - 0 1");
    invalidFen("8/8/8/8/8/8/8/8 w KK - 0 1");
    invalidFen("8/8/8/8/8/8/8/8 w - a1 0 1");
    invalidFen("8/8/8/8/8/8/8/8 w - - -1 1");
    invalidFen("8/8/8/8/8/8/8/8 w - - 0 0");
    invalidFen("8/8/8/8/8/8/8/9 w - - 0 1");
}

}

int main() {
    testHelpers();
    testStart();
    testFenState();
    testEmptyAndPieceTypes();
    testMalformedFen();
}
