#pragma once

#include "board.h"

namespace aggrochess {

// Attack masks include the first occupied square on every ray. They do not
// distinguish friendly from enemy occupancy; callers can mask as required.
[[nodiscard]] Bitboard pawnAttacks(Color color, Square square) noexcept;
[[nodiscard]] Bitboard knightAttacks(Square square) noexcept;
[[nodiscard]] Bitboard kingAttacks(Square square) noexcept;
[[nodiscard]] Bitboard bishopAttacks(Square square, Bitboard occupancy) noexcept;
[[nodiscard]] Bitboard rookAttacks(Square square, Bitboard occupancy) noexcept;
[[nodiscard]] Bitboard queenAttacks(Square square, Bitboard occupancy) noexcept;

[[nodiscard]] bool isSquareAttacked(const Position& position, Square square,
                                    Color byColor) noexcept;
[[nodiscard]] bool isInCheck(const Position& position, Color color);

}  // namespace aggrochess
