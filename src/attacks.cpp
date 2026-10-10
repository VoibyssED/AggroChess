#include "attacks.h"

#include <array>
#include <stdexcept>

namespace aggrochess {
namespace {

constexpr bool isOnBoard(int file, int rank) noexcept {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

constexpr Square makeSquare(int file, int rank) noexcept {
    return static_cast<Square>(rank * 8 + file);
}

constexpr Bitboard leaperAttacks(Square square,
                                  const int (&offsets)[8][2]) noexcept {
    Bitboard attacks = 0;
    const int file = squareIndex(square) % 8;
    const int rank = squareIndex(square) / 8;
    for (const auto& offset : offsets) {
        const int targetFile = file + offset[0];
        const int targetRank = rank + offset[1];
        if (isOnBoard(targetFile, targetRank)) {
            attacks |= squareMask(makeSquare(targetFile, targetRank));
        }
    }
    return attacks;
}

Bitboard slidingAttacks(Square square, Bitboard occupancy,
                        const int directions[][2], std::size_t count) noexcept {
    Bitboard attacks = 0;
    const int sourceFile = squareIndex(square) % 8;
    const int sourceRank = squareIndex(square) / 8;
    for (std::size_t direction = 0; direction < count; ++direction) {
        int file = sourceFile + directions[direction][0];
        int rank = sourceRank + directions[direction][1];
        while (isOnBoard(file, rank)) {
            const Square target = makeSquare(file, rank);
            attacks |= squareMask(target);
            if (testSquare(occupancy, target)) break;
            file += directions[direction][0];
            rank += directions[direction][1];
        }
    }
    return attacks;
}

}  // namespace

Bitboard pawnAttacks(Color color, Square square) noexcept {
    static constexpr auto tables = [] {
        std::array<std::array<Bitboard, 64>, 2> result{};
        for (int colorIndex = 0; colorIndex < 2; ++colorIndex) {
            const int rankStep = colorIndex == 0 ? 1 : -1;
            for (int index = 0; index < 64; ++index) {
                const int file = index % 8;
                const int rank = index / 8;
                for (const int fileStep : {-1, 1}) {
                    const int targetFile = file + fileStep;
                    const int targetRank = rank + rankStep;
                    if (isOnBoard(targetFile, targetRank)) {
                        result[static_cast<std::size_t>(colorIndex)]
                              [static_cast<std::size_t>(index)] |=
                            squareMask(makeSquare(targetFile, targetRank));
                    }
                }
            }
        }
        return result;
    }();
    return tables[static_cast<std::size_t>(color)]
                 [static_cast<std::size_t>(squareIndex(square))];
}

Bitboard knightAttacks(Square square) noexcept {
    static constexpr auto table = [] {
        std::array<Bitboard, 64> result{};
        constexpr int offsets[8][2] = {
            {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
            {1, -2}, {1, 2}, {2, -1}, {2, 1}
        };
        for (int index = 0; index < 64; ++index) {
            result[static_cast<std::size_t>(index)] =
                leaperAttacks(static_cast<Square>(index), offsets);
        }
        return result;
    }();
    return table[static_cast<std::size_t>(squareIndex(square))];
}

Bitboard kingAttacks(Square square) noexcept {
    static constexpr auto table = [] {
        std::array<Bitboard, 64> result{};
        constexpr int offsets[8][2] = {
            {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
            {0, 1}, {1, -1}, {1, 0}, {1, 1}
        };
        for (int index = 0; index < 64; ++index) {
            result[static_cast<std::size_t>(index)] =
                leaperAttacks(static_cast<Square>(index), offsets);
        }
        return result;
    }();
    return table[static_cast<std::size_t>(squareIndex(square))];
}

Bitboard bishopAttacks(Square square, Bitboard occupancy) noexcept {
    static constexpr int directions[4][2] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };
    return slidingAttacks(square, occupancy, directions, 4);
}

Bitboard rookAttacks(Square square, Bitboard occupancy) noexcept {
    static constexpr int directions[4][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    return slidingAttacks(square, occupancy, directions, 4);
}

Bitboard queenAttacks(Square square, Bitboard occupancy) noexcept {
    return bishopAttacks(square, occupancy) | rookAttacks(square, occupancy);
}

bool isSquareAttacked(const Position& position, Square square,
                      Color byColor) noexcept {
    Bitboard pawns = position.pieces(byColor, PieceType::Pawn);
    while (pawns != 0) {
        const Square pawn = popLeastSignificantBit(pawns);
        if (testSquare(pawnAttacks(byColor, pawn), square)) return true;
    }

    if ((knightAttacks(square) & position.pieces(byColor, PieceType::Knight)) != 0) {
        return true;
    }
    if ((kingAttacks(square) & position.pieces(byColor, PieceType::King)) != 0) {
        return true;
    }

    const Bitboard occupancy = position.allOccupancy();
    const Bitboard diagonalSliders = position.pieces(byColor, PieceType::Bishop) |
                                     position.pieces(byColor, PieceType::Queen);
    if ((bishopAttacks(square, occupancy) & diagonalSliders) != 0) return true;

    const Bitboard orthogonalSliders = position.pieces(byColor, PieceType::Rook) |
                                       position.pieces(byColor, PieceType::Queen);
    return (rookAttacks(square, occupancy) & orthogonalSliders) != 0;
}

bool isInCheck(const Position& position, Color color) {
    const Color attacker = color == Color::White ? Color::Black : Color::White;
    return isSquareAttacked(position, position.kingSquare(color), attacker);
}

}  // namespace aggrochess
