#include "movegen.h"
#include "zobrist.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace aggrochess;

namespace {

void assertIncrementalKey(const Position& position) {
    assert(position.zobristKey() == zobrist::compute(position));
    assert(position.isValid());
}

void verifyTree(Position& position, unsigned depth) {
    assertIncrementalKey(position);
    if (depth == 0) return;

    const std::string originalFen = position.toFen();
    const ZobristKey originalKey = position.zobristKey();
    for (const Move& move : generateLegalMoves(position)) {
        Undo undo;
        makeMove(position, move, undo);
        assertIncrementalKey(position);
        verifyTree(position, depth - 1);
        unmakeMove(position, undo);
        assert(position.toFen() == originalFen);
        assert(position.zobristKey() == originalKey);
        assertIncrementalKey(position);
    }
}

void testPositionComponents() {
    const Position start = Position::startPosition();
    assertIncrementalKey(start);
    assert(start.zobristKey() == 0xF6EC4D2D627AA669ULL);

    const Position differentCounters = Position::fromFen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 37 91");
    assert(start.zobristKey() == differentCounters.zobristKey());

    const Position blackToMove = Position::fromFen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    assert(start.zobristKey() != blackToMove.zobristKey());

    const Position noCastling = Position::fromFen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
    assert(start.zobristKey() != noCastling.zobristKey());

    const Position inertEp = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K3 w - d6 0 1");
    const Position noEp = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    assert(inertEp.zobristKey() == noEp.zobristKey());

    const Position actionableEp = Position::fromFen(
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const Position actionableBoardWithoutEp = Position::fromFen(
        "4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
    assert(actionableEp.zobristKey() != actionableBoardWithoutEp.zobristKey());
    assert(zobrist::enPassant(actionableEp) == zobrist::enPassantFile(3));

    // This nominal capture is pinned by the rook and therefore does not alter
    // the legal move set or repetition identity.
    const Position pinnedEp = Position::fromFen(
        "8/8/8/4KPpr/8/8/8/7k w - g6 0 1");
    const Position pinnedWithoutEp = Position::fromFen(
        "8/8/8/4KPpr/8/8/8/7k w - - 0 1");
    assert(pinnedEp.zobristKey() == pinnedWithoutEp.zobristKey());

    Position inertDoublePush = Position::startPosition();
    Undo inertUndo;
    makeMove(inertDoublePush,
             Move(Square::E2, Square::E4, MoveType::PawnDoublePush), inertUndo);
    const Position inertDoublePushWithoutEp = Position::fromFen(
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    assert(inertDoublePush.zobristKey() ==
           inertDoublePushWithoutEp.zobristKey());

    Position capturableDoublePush = Position::fromFen(
        "4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
    Undo capturableUndo;
    makeMove(capturableDoublePush,
             Move(Square::E2, Square::E4, MoveType::PawnDoublePush),
             capturableUndo);
    const Position capturableDoublePushWithoutEp = Position::fromFen(
        "4k3/8/8/8/3pP3/8/8/4K3 b - - 0 1");
    assert(capturableDoublePush.zobristKey() !=
           capturableDoublePushWithoutEp.zobristKey());

    const Position movedKing = Position::fromFen(
        "4k3/8/8/8/8/8/4K3/8 w - - 0 1");
    assert(noEp.zobristKey() != movedKing.zobristKey());
}

void testSpecialMoveUpdates() {
    struct Case {
        const char* fen;
        Move move;
    };
    const Case cases[] = {
        {"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1",
         Move(Square::E2, Square::E4, MoveType::PawnDoublePush)},
        {"4k3/8/8/8/3p4/8/4N3/4K3 w - - 0 1",
         Move(Square::E2, Square::D4, MoveType::Capture)},
        {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
         Move(Square::E5, Square::D6, MoveType::EnPassant)},
        {"4k3/8/8/8/8/8/8/4K2R w K - 0 1",
         Move(Square::E1, Square::G1, MoveType::Castle)},
        {"4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
         Move(Square::A7, Square::A8, MoveType::Promotion, PieceType::Knight)},
        {"1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",
         Move(Square::A7, Square::B8, MoveType::PromotionCapture,
              PieceType::Queen)},
        {"r3k3/8/8/8/8/8/8/R3K3 w Qq - 0 1",
         Move(Square::A1, Square::A8, MoveType::Capture)}
    };

    for (const Case& testCase : cases) {
        Position position = Position::fromFen(testCase.fen);
        const std::string before = position.toFen();
        const ZobristKey beforeKey = position.zobristKey();
        Undo undo;
        makeMove(position, testCase.move, undo);
        assertIncrementalKey(position);
        assert(position.zobristKey() != beforeKey);
        unmakeMove(position, undo);
        assert(position.toFen() == before);
        assert(position.zobristKey() == beforeKey);
        assertIncrementalKey(position);
    }
}

void testRejectedMovePreservesKey() {
    Position position = Position::startPosition();
    const ZobristKey key = position.zobristKey();
    const std::string fen = position.toFen();
    Undo undo;
    bool threw = false;
    try {
        makeMove(position, Move(Square::A3, Square::A4, MoveType::Quiet), undo);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(position.zobristKey() == key);
    assert(position.toFen() == fen);
}

void testPerftTreeConsistency() {
    Position start = Position::startPosition();
    verifyTree(start, 3);

    Position specialMoves = Position::fromFen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    verifyTree(specialMoves, 2);
}

void testDeterministicRandomWalks() {
    std::uint64_t randomState = 0x1884CEC9A66B0C5EULL;
    for (int game = 0; game < 32; ++game) {
        Position position = Position::startPosition();
        std::vector<Undo> undos;
        std::vector<std::string> fens;
        std::vector<ZobristKey> keys;

        for (int ply = 0; ply < 100; ++ply) {
            assertIncrementalKey(position);
            const MoveList moves = generateLegalMoves(position);
            if (moves.empty()) break;
            randomState = randomState * 6364136223846793005ULL +
                          1442695040888963407ULL;
            const std::size_t choice =
                static_cast<std::size_t>(randomState % moves.size());
            fens.push_back(position.toFen());
            keys.push_back(position.zobristKey());
            undos.emplace_back();
            makeMove(position, moves[choice], undos.back());
        }

        while (!undos.empty()) {
            unmakeMove(position, undos.back());
            undos.pop_back();
            assert(position.toFen() == fens.back());
            assert(position.zobristKey() == keys.back());
            assertIncrementalKey(position);
            fens.pop_back();
            keys.pop_back();
        }
        assert(position.toFen() == Position::startPosition().toFen());
    }
}

}  // namespace

int main() {
    testPositionComponents();
    testSpecialMoveUpdates();
    testRejectedMovePreservesKey();
    testPerftTreeConsistency();
    testDeterministicRandomWalks();
}
