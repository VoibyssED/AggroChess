#include "movegen.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace aggrochess;

namespace {

void expectInvalidUci(const Position& position, std::string_view value) {
    bool threw = false;
    try { (void)moveFromUci(position, value); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testMoveIdentityAndEncoding() {
    static_assert(sizeof(Move) == 4);

    const Move invalid;
    assert(!invalid.isValid());
    assert(!invalid.isNull());
    assert(invalid.type() == MoveType::Invalid);
    assert(invalid.raw() == 0);
    assert(Move::fromRaw(invalid.raw()) == invalid);
    assert(!Move::fromRaw(0xFFFFU).isValid());

    const Move nullMove = Move::null();
    assert(nullMove.isValid());
    assert(nullMove.isNull());
    assert(nullMove.type() == MoveType::Null);
    assert(Move::fromRaw(nullMove.raw()) == nullMove);
    assert(nullMove.raw() == 0x1000U);
    assert(moveToUci(nullMove) == "0000");

    const Move quiet(Square::G1, Square::F3, MoveType::Quiet);
    assert(quiet.isValid() && !quiet.isNull());
    assert(quiet.from() == Square::G1 && quiet.to() == Square::F3);
    assert(quiet.type() == MoveType::Quiet);
    assert(quiet.raw() == 0x2546U);
    assert(Move::fromRaw(quiet.raw()) == quiet);
    assert(quiet == Move(Square::G1, Square::F3, MoveType::Quiet,
                         PieceType::Knight));
    assert(quiet != Move(Square::G1, Square::F3, MoveType::Capture));

    for (const PieceType piece : {PieceType::Knight, PieceType::Bishop,
                                  PieceType::Rook, PieceType::Queen}) {
        const Move promotion(Square::A7, Square::A8, MoveType::Promotion, piece);
        const Move capture(Square::A7, Square::B8,
                           MoveType::PromotionCapture, piece);
        assert(promotion.type() == MoveType::Promotion);
        assert(capture.type() == MoveType::PromotionCapture);
        assert(promotion.promotion() == piece);
        assert(capture.promotion() == piece);
        assert(Move::fromRaw(promotion.raw()) == promotion);
        assert(Move::fromRaw(capture.raw()) == capture);
    }

    assert(!Move(Square::A7, Square::A8, MoveType::Promotion,
                 PieceType::King).isValid());
    assert(!Move(Square::None, Square::A1, MoveType::Quiet).isValid());
}

void testBasicParsing() {
    const Position start = Position::startPosition();
    const std::string fen = start.toFen();
    const ZobristKey key = start.zobristKey();

    const Move e4 = moveFromUci(start, "e2e4");
    assert(e4 == Move(Square::E2, Square::E4, MoveType::PawnDoublePush));
    assert(moveToUci(e4) == "e2e4");

    const Move knight = moveFromUci(start, "g1f3");
    assert(knight == Move(Square::G1, Square::F3, MoveType::Quiet));
    assert(moveToUci(knight) == "g1f3");
    assert(start.toFen() == fen);
    assert(start.zobristKey() == key);
}

void testSpecialMoveParsing() {
    const Position capture = Position::fromFen(
        "4k3/8/8/8/3p4/8/4N3/4K3 w - - 0 1");
    assert(moveFromUci(capture, "e2d4").type() == MoveType::Capture);

    const Position enPassant = Position::fromFen(
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    assert(moveFromUci(enPassant, "e5d6").type() == MoveType::EnPassant);

    const Position castling = Position::fromFen(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    assert(moveFromUci(castling, "e1g1").type() == MoveType::Castle);
    assert(moveFromUci(castling, "e1c1").type() == MoveType::Castle);

    const Position promotions = Position::fromFen(
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    for (const char suffix : {'n', 'b', 'r', 'q'}) {
        const std::string quiet = std::string("a7a8") + suffix;
        const std::string captured = std::string("a7b8") + suffix;
        assert(moveToUci(moveFromUci(promotions, quiet)) == quiet);
        assert(moveToUci(moveFromUci(promotions, captured)) == captured);
    }
}

void testIllegalAndMalformedInput() {
    const Position start = Position::startPosition();
    for (const std::string_view value : {
             "", "e2", "e2e4q", "e2e4x", "E2E4", "i2e4", "e9e4",
             "e2e5", "e7e5", "e2e4q", "0000"}) {
        expectInvalidUci(start, value);
    }

    const Position pinned = Position::fromFen(
        "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    expectInvalidUci(pinned, "e2d2");

    const Position promotion = Position::fromFen(
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    expectInvalidUci(promotion, "a7a8");

    bool threw = false;
    try { (void)moveToUci(Move{}); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);

    Position mutableStart = Position::startPosition();
    const std::string before = mutableStart.toFen();
    Undo undo;
    threw = false;
    try { makeMove(mutableStart, Move::null(), undo); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    assert(mutableStart.toFen() == before);
}

void testGeneratedMoveRoundTrips() {
    const Position positions[] = {
        Position::startPosition(),
        Position::fromFen(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"),
        Position::fromFen(
            "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"),
        Position::fromFen(
            "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1")
    };

    for (const Position& position : positions) {
        for (const Move& move : generateLegalMoves(position)) {
            assert(Move::fromRaw(move.raw()) == move);
            assert(moveFromUci(position, moveToUci(move)) == move);
        }
    }
}

}  // namespace

int main() {
    testMoveIdentityAndEncoding();
    testBasicParsing();
    testSpecialMoveParsing();
    testIllegalAndMalformedInput();
    testGeneratedMoveRoundTrips();
}
