#pragma once

#include "board.h"

#include <cstdint>

namespace aggrochess {
namespace zobrist {

// Keys are generated deterministically from a fixed seed, so identical
// positions have identical keys across runs, builds, and platforms.
[[nodiscard]] ZobristKey piece(Color color, PieceType type,
                               Square square) noexcept;
[[nodiscard]] ZobristKey sideToMove() noexcept;
[[nodiscard]] ZobristKey castlingRights(std::uint8_t rights) noexcept;
[[nodiscard]] ZobristKey enPassantFile(int file) noexcept;
// Returns the en-passant component only when the side to move has at least one
// legal en-passant capture. This matches repetition-position identity.
[[nodiscard]] ZobristKey enPassant(const Position& position) noexcept;
[[nodiscard]] ZobristKey compute(const Position& position) noexcept;

}  // namespace zobrist
}  // namespace aggrochess
