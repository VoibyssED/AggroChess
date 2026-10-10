#pragma once

#include "board.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aggrochess {

using MoveList = std::vector<Move>;

// Returns only moves that obey piece movement rules and leave the moving
// side's king safe. Castling, promotion, and en-passant rules are included.
[[nodiscard]] MoveList generateLegalMoves(const Position& position);

// UCI long algebraic notation, for example e2e4 or a7a8q. Null moves serialize
// as 0000. Parsing resolves against the legal move list and rejects null,
// malformed, and illegal external moves.
[[nodiscard]] std::string moveToUci(const Move& move);
[[nodiscard]] Move moveFromUci(const Position& position, std::string_view value);

// Primarily intended for correctness testing and diagnostics. The position is
// restored before this function returns.
[[nodiscard]] std::uint64_t perft(Position& position, unsigned depth);

}  // namespace aggrochess
