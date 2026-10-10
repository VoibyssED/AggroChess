#pragma once

#include "board.h"

namespace aggrochess {

[[nodiscard]] constexpr int pieceValue(PieceType type) noexcept {
    switch (type) {
    case PieceType::Pawn: return 100;
    case PieceType::Knight: return 320;
    case PieceType::Bishop: return 330;
    case PieceType::Rook: return 500;
    case PieceType::Queen: return 900;
    case PieceType::King: return 20000;
    case PieceType::Count: return 0;
    }
    return 0;
}

// Returns the material result in centipawns if move is played and both sides
// then choose captures on its destination square optimally. Either side may
// decline an unfavorable continuation. The move must be structurally valid
// for the position; illegal king-exposing moves are rejected.
[[nodiscard]] int staticExchangeEvaluation(const Position& position,
                                           const Move& move);

}  // namespace aggrochess
