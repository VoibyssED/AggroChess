#include "zobrist.h"

#include "attacks.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace aggrochess::zobrist {
namespace {

constexpr std::uint64_t seed = 0xA66B0C5E5510B157ULL;
constexpr std::size_t pieceKeyCount = 2U * 6U * 64U;
constexpr std::size_t castlingKeyOffset = pieceKeyCount;
constexpr std::size_t enPassantKeyOffset = castlingKeyOffset + 4U;
constexpr std::size_t sideKeyOffset = enPassantKeyOffset + 8U;
constexpr std::size_t keyCount = sideKeyOffset + 1U;

constexpr std::uint64_t splitmix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

constexpr std::array<ZobristKey, keyCount> makeKeys() noexcept {
    std::array<ZobristKey, keyCount> keys{};
    std::uint64_t state = seed;
    for (ZobristKey& key : keys) {
        state = splitmix64(state);
        key = state;
    }
    return keys;
}

constexpr auto keys = makeKeys();

constexpr std::size_t colorIndex(Color color) noexcept {
    return static_cast<std::size_t>(color);
}

constexpr std::size_t pieceIndex(PieceType type) noexcept {
    return static_cast<std::size_t>(type);
}

constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black : Color::White;
}

bool attackedWithOccupancy(const Position& position, Square square,
                           Color byColor, Bitboard occupancy,
                           Bitboard pawns) noexcept {
    while (pawns != 0) {
        const Square pawn = static_cast<Square>(std::countr_zero(pawns));
        pawns &= pawns - 1;
        if (testSquare(pawnAttacks(byColor, pawn), square)) return true;
    }
    if ((knightAttacks(square) &
         position.pieces(byColor, PieceType::Knight)) != 0) return true;
    if ((kingAttacks(square) &
         position.pieces(byColor, PieceType::King)) != 0) return true;
    if ((bishopAttacks(square, occupancy) &
         (position.pieces(byColor, PieceType::Bishop) |
          position.pieces(byColor, PieceType::Queen))) != 0) return true;
    return (rookAttacks(square, occupancy) &
            (position.pieces(byColor, PieceType::Rook) |
             position.pieces(byColor, PieceType::Queen))) != 0;
}

}  // namespace

ZobristKey piece(Color color, PieceType type, Square square) noexcept {
    const std::size_t index =
        (colorIndex(color) * 6U + pieceIndex(type)) * 64U +
        static_cast<std::size_t>(squareIndex(square));
    return keys[index];
}

ZobristKey sideToMove() noexcept {
    return keys[sideKeyOffset];
}

ZobristKey castlingRights(std::uint8_t rights) noexcept {
    ZobristKey key = 0;
    if ((rights & WhiteKingSide) != 0) key ^= keys[castlingKeyOffset];
    if ((rights & WhiteQueenSide) != 0) key ^= keys[castlingKeyOffset + 1U];
    if ((rights & BlackKingSide) != 0) key ^= keys[castlingKeyOffset + 2U];
    if ((rights & BlackQueenSide) != 0) key ^= keys[castlingKeyOffset + 3U];
    return key;
}

ZobristKey enPassantFile(int file) noexcept {
    return keys[enPassantKeyOffset + static_cast<std::size_t>(file)];
}

ZobristKey enPassant(const Position& position) noexcept {
    if (!position.hasEnPassantSquare()) return 0;

    const Color movingColor = position.sideToMove();
    const Color enemyColor = opposite(movingColor);
    const Square target = position.enPassantSquare();
    const int step = movingColor == Color::White ? 8 : -8;
    const int capturedIndex = squareIndex(target) - step;
    if (capturedIndex < 0 || capturedIndex >= 64 ||
        testSquare(position.allOccupancy(), target)) return 0;
    const Square capturedSquare = static_cast<Square>(capturedIndex);
    if (!testSquare(position.pieces(enemyColor, PieceType::Pawn), capturedSquare)) {
        return 0;
    }

    Bitboard candidates = position.pieces(movingColor, PieceType::Pawn) &
                          pawnAttacks(enemyColor, target);
    if (candidates == 0) return 0;
    const Bitboard kings = position.pieces(movingColor, PieceType::King);
    const bool hasSingleKing = countBits(kings) == 1;
    const Square king = hasSingleKing
        ? static_cast<Square>(std::countr_zero(kings)) : Square::None;
    while (candidates != 0) {
        const Square from = static_cast<Square>(std::countr_zero(candidates));
        candidates &= candidates - 1;
        if (!hasSingleKing) {
            return enPassantFile(squareIndex(target) % 8);
        }

        Bitboard occupancy = position.allOccupancy();
        clearSquare(occupancy, from);
        clearSquare(occupancy, capturedSquare);
        setSquare(occupancy, target);
        const Bitboard enemyPawns =
            position.pieces(enemyColor, PieceType::Pawn) & ~squareMask(capturedSquare);
        if (!attackedWithOccupancy(position, king, enemyColor, occupancy,
                                   enemyPawns)) {
            return enPassantFile(squareIndex(target) % 8);
        }
    }
    return 0;
}

ZobristKey compute(const Position& position) noexcept {
    ZobristKey key = 0;
    for (const Color color : {Color::White, Color::Black}) {
        for (const PieceType type : {PieceType::Pawn, PieceType::Knight,
                                     PieceType::Bishop, PieceType::Rook,
                                     PieceType::Queen, PieceType::King}) {
            Bitboard pieces = position.pieces(color, type);
            while (pieces != 0) {
                const int index = static_cast<int>(std::countr_zero(pieces));
                key ^= piece(color, type, static_cast<Square>(index));
                pieces &= pieces - 1;
            }
        }
    }
    if (position.sideToMove() == Color::Black) key ^= sideToMove();
    key ^= castlingRights(position.castlingRights());
    key ^= enPassant(position);
    return key;
}

}  // namespace aggrochess::zobrist
