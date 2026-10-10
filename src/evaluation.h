#pragma once

#include "board.h"

namespace aggrochess {

struct EvalTerm {
    int middlegame = 0;
    int endgame = 0;
};

struct EvaluationBreakdown {
    EvalTerm material;
    EvalTerm pieceSquare;
    EvalTerm mobility;
    EvalTerm pawnStructure;
    EvalTerm bishopPair;
    EvalTerm rookActivity;
    EvalTerm kingSafety;
    EvalTerm space;
    int phase = 0;              // 0 = pure endgame, 24 = full middlegame.
    int whitePerspective = 0;   // Positive values favor White.
    int sideToMoveScore = 0;    // Positive values favor the side to move.
};

[[nodiscard]] EvaluationBreakdown evaluateDetailed(const Position& position);
[[nodiscard]] int evaluate(const Position& position);

}  // namespace aggrochess
