#include "engine.h"

#include "movegen.h"

#include <cassert>
#include <sstream>
#include <string>

using namespace aggrochess;

namespace {

void testPositionCommands() {
    Engine engine;
    engine.setPosition("position startpos moves e2e4 e7e5 g1f3");
    assert(engine.position().toFen() ==
           "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");

    engine.setPosition(
        "position fen 4k3/8/8/8/8/8/R7/4K3 w - - 0 1 moves a2a8");
    assert(engine.position().toFen() ==
           "R3k3/8/8/8/8/8/8/4K3 b - - 1 1");
}

void testOnePlyMoveIsLegal() {
    Engine engine;
    int score = 0;
    const Move move = engine.chooseMove(&score);
    assert(move.isValid() && !move.isNull());
    assert(moveFromUci(engine.position(), moveToUci(move)) == move);

    engine.setPosition(
        "position fen rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    assert(engine.chooseMove().isNull());
}

void testUciSession() {
    std::istringstream input(
        "uci\n"
        "isready\n"
        "setoption name Hash value 1\n"
        "ucinewgame\n"
        "position startpos moves e2e4\n"
        "eval\n"
        "go depth 1\n"
        "quit\n");
    std::ostringstream output;
    runUci(input, output);
    const std::string text = output.str();
    assert(text.find("id name AggroChess") != std::string::npos);
    assert(text.find("option name Hash") != std::string::npos);
    assert(text.find("uciok") != std::string::npos);
    assert(text.find("readyok") != std::string::npos);
    assert(text.find("info string eval") != std::string::npos);
    assert(text.find("info depth 1 score cp") != std::string::npos);
    assert(text.find("bestmove ") != std::string::npos);
}

void testUciErrorsAndNoMove() {
    std::istringstream input(
        "position nonsense\n"
        "position fen rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3\n"
        "go\n"
        "quit\n");
    std::ostringstream output;
    runUci(input, output);
    assert(output.str().find("info string error:") != std::string::npos);
    assert(output.str().find("bestmove 0000") != std::string::npos);
}

}  // namespace

int main() {
    testPositionCommands();
    testOnePlyMoveIsLegal();
    testUciSession();
    testUciErrorsAndNoMove();
}
