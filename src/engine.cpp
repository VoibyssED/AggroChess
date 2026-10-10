#include "engine.h"

#include "attacks.h"
#include "evaluation.h"
#include "movegen.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aggrochess {
namespace {

std::vector<std::string> tokens(std::string_view text) {
    std::istringstream stream{std::string(text)};
    std::vector<std::string> result;
    std::string token;
    while (stream >> token) result.push_back(token);
    return result;
}

}  // namespace

Engine::Engine()
    : position_(Position::startPosition()), history_(position_), table_(16) {}

void Engine::newGame() {
    position_ = Position::startPosition();
    history_.reset(position_);
    table_.clear();
    table_.newSearch();
}

void Engine::setHashSize(std::size_t megabytes) {
    if (megabytes < 1 || megabytes > 1024) {
        throw std::out_of_range("Hash must be between 1 and 1024 MB");
    }
    table_.resize(megabytes);
}

void Engine::setPosition(std::string_view command) {
    const auto words = tokens(command);
    if (words.empty() || words[0] != "position") {
        throw std::invalid_argument("position command must start with 'position'");
    }

    std::size_t index = 1;
    Position next;
    if (index < words.size() && words[index] == "startpos") {
        next = Position::startPosition();
        ++index;
    } else if (index < words.size() && words[index] == "fen") {
        ++index;
        if (words.size() - index < 6) {
            throw std::invalid_argument("position fen requires six FEN fields");
        }
        std::string fen;
        for (int field = 0; field < 6; ++field) {
            if (field != 0) fen += ' ';
            fen += words[index++];
        }
        next = Position::fromFen(fen);
    } else {
        throw std::invalid_argument("position requires startpos or fen");
    }

    PositionHistory nextHistory(next);
    if (index < words.size()) {
        if (words[index] != "moves") {
            throw std::invalid_argument("unexpected text after position");
        }
        ++index;
        while (index < words.size()) {
            const Move move = moveFromUci(next, words[index++]);
            Undo undo;
            makeMove(next, move, undo);
            nextHistory.push(next);
        }
    }
    position_ = next;
    history_ = std::move(nextHistory);
}

Move Engine::chooseMove(int* score) const {
    const MoveList moves = generateLegalMoves(position_);
    if (moves.empty()) {
        if (score != nullptr) *score = 0;
        return Move::null();
    }

    Move best = moves.front();
    int bestScore = -MateScore;
    for (const Move& move : moves) {
        Position next = position_;
        Undo undo;
        makeMove(next, move, undo);
        PositionHistory nextHistory = history_;
        nextHistory.push(next);

        int candidate = 0;
        const MoveList replies = generateLegalMoves(next);
        if (replies.empty()) {
            candidate = isInCheck(next, next.sideToMove()) ? MateScore - 1 : 0;
        } else if (isInsufficientMaterial(next) || isFiftyMoveDraw(next) ||
                   nextHistory.isThreefoldRepetition(next)) {
            candidate = 0;
        } else {
            candidate = -evaluate(next);
        }

        if (candidate > bestScore) {
            bestScore = candidate;
            best = move;
        }
    }
    if (score != nullptr) *score = bestScore;
    return best;
}

void runUci(std::istream& input, std::ostream& output) {
    Engine engine;
    std::string line;
    while (std::getline(input, line)) {
        try {
            if (line == "uci") {
                output << "id name AggroChess 0.1\n"
                       << "id author VoibyssED\n"
                       << "option name Hash type spin default 16 min 1 max 1024\n"
                       << "uciok\n";
            } else if (line == "isready") {
                output << "readyok\n";
            } else if (line == "ucinewgame") {
                engine.newGame();
            } else if (line.rfind("setoption", 0) == 0) {
                const auto words = tokens(line);
                if (words.size() == 5 && words[1] == "name" &&
                    words[2] == "Hash" && words[3] == "value") {
                    engine.setHashSize(static_cast<std::size_t>(std::stoull(words[4])));
                }
            } else if (line.rfind("position", 0) == 0) {
                engine.setPosition(line);
            } else if (line.rfind("go", 0) == 0) {
                int score = 0;
                const Move move = engine.chooseMove(&score);
                if (!move.isNull()) {
                    output << "info depth 1 score cp " << score
                           << " nodes " << generateLegalMoves(engine.position()).size()
                           << " pv " << moveToUci(move) << '\n';
                }
                output << "bestmove " << moveToUci(move) << '\n';
            } else if (line == "eval") {
                const EvaluationBreakdown evaluation =
                    evaluateDetailed(engine.position());
                output << "info string eval " << evaluation.sideToMoveScore
                       << " phase " << evaluation.phase << '\n';
            } else if (line == "stop" || line == "ponderhit") {
                // The current one-ply chooser is synchronous and finishes
                // before another command can be consumed.
            } else if (line == "quit") {
                break;
            }
        } catch (const std::exception& error) {
            output << "info string error: " << error.what() << '\n';
        }
        output.flush();
    }
}

}  // namespace aggrochess
