#include "movegen.h"

#include "attacks.h"

#include <cassert>

using namespace aggrochess;

namespace {

bool containsMove(const MoveList& moves, Square from, Square to, MoveType type,
                  PieceType promotion = PieceType::Queen) {
    const bool isPromotion = type == MoveType::Promotion ||
                             type == MoveType::PromotionCapture;
    for (const Move& move : moves) {
        if (move.from() == from && move.to() == to && move.type() == type &&
            (!isPromotion || move.promotion() == promotion)) {
            return true;
        }
    }
    return false;
}

void testInitialMoves() {
    const Position position = Position::startPosition();
    const MoveList moves = generateLegalMoves(position);
    assert(moves.size() == 20);
    assert(containsMove(moves, Square::E2, Square::E4,
                        MoveType::PawnDoublePush));
    assert(containsMove(moves, Square::G1, Square::F3, MoveType::Quiet));
    assert(!containsMove(moves, Square::E1, Square::G1, MoveType::Castle));
}

void testPinsAndChecks() {
    const Position pinned = Position::fromFen(
        "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const MoveList pinnedMoves = generateLegalMoves(pinned);
    assert(!containsMove(pinnedMoves, Square::E2, Square::D2, MoveType::Quiet));
    assert(containsMove(pinnedMoves, Square::E2, Square::E8, MoveType::Capture));

    const Position checkmate = Position::fromFen(
        "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    assert(isInCheck(checkmate, Color::White));
    assert(generateLegalMoves(checkmate).empty());
}

void testCastlingRules() {
    const Position open = Position::fromFen(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const MoveList openMoves = generateLegalMoves(open);
    assert(containsMove(openMoves, Square::E1, Square::G1, MoveType::Castle));
    assert(containsMove(openMoves, Square::E1, Square::C1, MoveType::Castle));

    const Position throughCheck = Position::fromFen(
        "k4r2/8/8/8/8/8/8/4K2R w K - 0 1");
    assert(!containsMove(generateLegalMoves(throughCheck),
                         Square::E1, Square::G1, MoveType::Castle));

    const Position noRights = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K2R w - - 0 1");
    assert(!containsMove(generateLegalMoves(noRights),
                         Square::E1, Square::G1, MoveType::Castle));
}

void testEnPassantLegality() {
    const Position ordinary = Position::fromFen(
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    assert(containsMove(generateLegalMoves(ordinary),
                        Square::E5, Square::D6, MoveType::EnPassant));

    // Moving f5 and removing g5 would expose the white king to the h5 rook.
    const Position discoveredCheck = Position::fromFen(
        "8/8/8/4KPpr/8/8/8/7k w - g6 0 1");
    assert(!containsMove(generateLegalMoves(discoveredCheck),
                         Square::F5, Square::G6, MoveType::EnPassant));
}

void testPromotions() {
    const Position position = Position::fromFen(
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    const MoveList moves = generateLegalMoves(position);
    for (const PieceType piece : {PieceType::Knight, PieceType::Bishop,
                                  PieceType::Rook, PieceType::Queen}) {
        assert(containsMove(moves, Square::A7, Square::A8,
                            MoveType::Promotion, piece));
        assert(containsMove(moves, Square::A7, Square::B8,
                            MoveType::PromotionCapture, piece));
    }
}

void testPerft() {
    Position initial = Position::startPosition();
    const std::string initialFen = initial.toFen();
    assert(perft(initial, 0) == 1);
    assert(perft(initial, 1) == 20);
    assert(perft(initial, 2) == 400);
    assert(perft(initial, 3) == 8902);
    assert(perft(initial, 4) == 197281);
    assert(initial.toFen() == initialFen);

    Position kiwipete = Position::fromFen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    const std::string kiwipeteFen = kiwipete.toFen();
    assert(perft(kiwipete, 1) == 48);
    assert(perft(kiwipete, 2) == 2039);
    assert(perft(kiwipete, 3) == 97862);
    assert(kiwipete.toFen() == kiwipeteFen);

    Position enPassantAndChecks = Position::fromFen(
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    assert(perft(enPassantAndChecks, 1) == 14);
    assert(perft(enPassantAndChecks, 2) == 191);
    assert(perft(enPassantAndChecks, 3) == 2812);

    Position promotionsAndCastling = Position::fromFen(
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    assert(perft(promotionsAndCastling, 1) == 6);
    assert(perft(promotionsAndCastling, 2) == 264);
    assert(perft(promotionsAndCastling, 3) == 9467);

    Position mirroredPromotionsAndCastling = Position::fromFen(
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
    assert(perft(mirroredPromotionsAndCastling, 1) == 6);
    assert(perft(mirroredPromotionsAndCastling, 2) == 264);
    assert(perft(mirroredPromotionsAndCastling, 3) == 9467);

    Position tactical = Position::fromFen(
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    assert(perft(tactical, 1) == 44);
    assert(perft(tactical, 2) == 1486);
    assert(perft(tactical, 3) == 62379);

    Position middlegame = Position::fromFen(
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
    assert(perft(middlegame, 1) == 46);
    assert(perft(middlegame, 2) == 2079);
    assert(perft(middlegame, 3) == 89890);
}

}  // namespace

int main() {
    testInitialMoves();
    testPinsAndChecks();
    testCastlingRules();
    testEnPassantLegality();
    testPromotions();
    testPerft();
}
