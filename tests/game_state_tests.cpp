#include "game_state.h"

#include "movegen.h"

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace aggrochess;

namespace {

void play(Position& position, PositionHistory& history, std::string_view uci,
          std::vector<Undo>& undos) {
    const Move move = moveFromUci(position, uci);
    undos.emplace_back();
    makeMove(position, move, undos.back());
    history.push(position);
}

void testCheckmateAndStalemate() {
    const Position checkmate = Position::fromFen(
        "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 100 3");
    assert(classifyGameState(checkmate) == GameStatus::Checkmate);

    const Position stalemate = Position::fromFen(
        "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    assert(classifyGameState(stalemate) == GameStatus::Stalemate);

    assert(classifyGameState(Position::startPosition()) == GameStatus::Ongoing);
}

void testFiftyMoveRule() {
    const Position before = Position::fromFen(
        "4k3/8/8/8/8/8/R7/4K3 w - - 99 70");
    const Position claimable = Position::fromFen(
        "4k3/8/8/8/8/8/R7/4K3 w - - 100 70");
    assert(!isFiftyMoveDraw(before));
    assert(isFiftyMoveDraw(claimable));
    assert(classifyGameState(before) == GameStatus::Ongoing);
    assert(classifyGameState(claimable) == GameStatus::DrawByFiftyMoveRule);
}

void testInsufficientMaterial() {
    const Position kings = Position::fromFen(
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    const Position bishop = Position::fromFen(
        "4k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    const Position knight = Position::fromFen(
        "4k3/8/8/8/8/8/8/2N1K3 w - - 0 1");
    const Position sameColorBishops = Position::fromFen(
        "4k3/8/8/8/5b2/8/8/2B1K3 w - - 0 1");
    const Position oppositeColorBishops = Position::fromFen(
        "2b1k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    const Position twoKnights = Position::fromFen(
        "4k3/8/8/8/8/8/8/1NN1K3 w - - 0 1");
    const Position pawn = Position::fromFen(
        "4k3/8/8/8/8/8/P7/4K3 w - - 0 1");

    assert(isInsufficientMaterial(kings));
    assert(isInsufficientMaterial(bishop));
    assert(isInsufficientMaterial(knight));
    assert(isInsufficientMaterial(sameColorBishops));
    assert(!isInsufficientMaterial(oppositeColorBishops));
    assert(!isInsufficientMaterial(twoKnights));
    assert(!isInsufficientMaterial(pawn));
    assert(classifyGameState(kings) == GameStatus::DrawByInsufficientMaterial);
}

void testThreefoldRepetition() {
    Position position = Position::startPosition();
    PositionHistory history(position);
    std::vector<Undo> undos;
    constexpr std::string_view cycle[] = {"g1f3", "g8f6", "f3g1", "f6g8"};

    for (const std::string_view move : cycle) play(position, history, move, undos);
    assert(history.repetitionCount(position) == 2);
    assert(!history.isThreefoldRepetition(position));
    assert(classifyGameState(position, &history) == GameStatus::Ongoing);

    for (const std::string_view move : cycle) play(position, history, move, undos);
    assert(history.repetitionCount(position) == 3);
    assert(history.isThreefoldRepetition(position));
    assert(classifyGameState(position, &history) ==
           GameStatus::DrawByThreefoldRepetition);

    history.pop();
    unmakeMove(position, undos.back());
    undos.pop_back();
    assert(history.currentKey() == position.zobristKey());
    assert(!history.isThreefoldRepetition(position));

    while (!undos.empty()) {
        history.pop();
        unmakeMove(position, undos.back());
        undos.pop_back();
    }
    assert(history.size() == 1);
    assert(history.currentKey() == Position::startPosition().zobristKey());
}

void testIrreversibleBoundaryAndHistoryErrors() {
    Position position = Position::startPosition();
    PositionHistory history(position);
    std::vector<Undo> undos;
    play(position, history, "g1f3", undos);
    play(position, history, "g8f6", undos);
    play(position, history, "f3g1", undos);
    play(position, history, "f6g8", undos);
    assert(history.repetitionCount(position) == 2);

    play(position, history, "e2e4", undos);
    assert(position.halfmoveClock() == 0);
    assert(history.repetitionCount(position) == 1);

    PositionHistory empty;
    bool threw = false;
    try { (void)empty.currentKey(); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
    threw = false;
    try { empty.pop(); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
}

}  // namespace

int main() {
    testCheckmateAndStalemate();
    testFiftyMoveRule();
    testInsufficientMaterial();
    testThreefoldRepetition();
    testIrreversibleBoundaryAndHistoryErrors();
}
