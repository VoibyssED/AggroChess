#include "evaluation.h"

#include "attacks.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <stdexcept>

namespace aggrochess {
namespace {

constexpr std::array<EvalTerm, 6> materialValues = {{
    {100, 120}, {320, 300}, {330, 320},
    {500, 510}, {900, 900}, {0, 0}
}};
constexpr std::array<int, 6> phaseWeights = {0, 1, 1, 2, 4, 0};
constexpr int maxPhase = 24;

constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black : Color::White;
}

constexpr int sign(Color color) noexcept {
    return color == Color::White ? 1 : -1;
}

constexpr int relativeRank(Color color, Square square) noexcept {
    const int rank = squareIndex(square) / 8;
    return color == Color::White ? rank : 7 - rank;
}

constexpr int centrality(Square square) noexcept {
    const int file = squareIndex(square) % 8;
    const int rank = squareIndex(square) / 8;
    return std::min(file, 7 - file) + std::min(rank, 7 - rank);
}

EvalTerm pieceSquareValue(PieceType type, Color color, Square square) noexcept {
    const int advance = relativeRank(color, square);
    const int center = centrality(square);
    switch (type) {
    case PieceType::Pawn:   return {advance * 7 + center * 2,
                                    advance * 12 + center};
    case PieceType::Knight: return {center * 12 - advance, center * 8};
    case PieceType::Bishop: return {center * 7 + advance * 2, center * 6};
    case PieceType::Rook:   return {advance * 2, advance * 4};
    case PieceType::Queen:  return {center * 3, center * 4};
    case PieceType::King:   return {-center * 10 - advance * 3, center * 12};
    case PieceType::Count:  return {};
    }
    return {};
}

void add(EvalTerm& destination, EvalTerm value, int multiplier = 1) noexcept {
    destination.middlegame += value.middlegame * multiplier;
    destination.endgame += value.endgame * multiplier;
}

Bitboard attacksFrom(const Position& position, Color color, PieceType type,
                     Square square) noexcept {
    switch (type) {
    case PieceType::Pawn: return pawnAttacks(color, square);
    case PieceType::Knight: return knightAttacks(square);
    case PieceType::Bishop: return bishopAttacks(square, position.allOccupancy());
    case PieceType::Rook: return rookAttacks(square, position.allOccupancy());
    case PieceType::Queen: return queenAttacks(square, position.allOccupancy());
    case PieceType::King: return kingAttacks(square);
    case PieceType::Count: return 0;
    }
    return 0;
}

Bitboard attackMap(const Position& position, Color color) noexcept {
    Bitboard result = 0;
    for (const PieceType type : {PieceType::Pawn, PieceType::Knight,
                                 PieceType::Bishop, PieceType::Rook,
                                 PieceType::Queen, PieceType::King}) {
        Bitboard pieces = position.pieces(color, type);
        while (pieces != 0) {
            const Square square = static_cast<Square>(std::countr_zero(pieces));
            pieces &= pieces - 1;
            result |= attacksFrom(position, color, type, square);
        }
    }
    return result;
}

bool isPassedPawn(const Position& position, Color color, Square square) noexcept {
    const Color enemy = opposite(color);
    const int file = squareIndex(square) % 8;
    const int rank = squareIndex(square) / 8;
    Bitboard enemyPawns = position.pieces(enemy, PieceType::Pawn);
    while (enemyPawns != 0) {
        const Square enemySquare = static_cast<Square>(std::countr_zero(enemyPawns));
        enemyPawns &= enemyPawns - 1;
        const int enemyFile = squareIndex(enemySquare) % 8;
        const int enemyRank = squareIndex(enemySquare) / 8;
        if (std::abs(enemyFile - file) <= 1 &&
            (color == Color::White ? enemyRank > rank : enemyRank < rank)) {
            return false;
        }
    }
    return true;
}

void evaluatePawns(const Position& position, Color color,
                   EvaluationBreakdown& result) noexcept {
    const int multiplier = sign(color);
    const Bitboard pawns = position.pieces(color, PieceType::Pawn);
    std::array<int, 8> fileCounts{};
    Bitboard remaining = pawns;
    while (remaining != 0) {
        const Square square = static_cast<Square>(std::countr_zero(remaining));
        remaining &= remaining - 1;
        ++fileCounts[static_cast<std::size_t>(squareIndex(square) % 8)];
    }

    for (int file = 0; file < 8; ++file) {
        if (fileCounts[static_cast<std::size_t>(file)] > 1) {
            const int extras = fileCounts[static_cast<std::size_t>(file)] - 1;
            add(result.pawnStructure, {-14 * extras, -20 * extras}, multiplier);
        }
    }

    remaining = pawns;
    while (remaining != 0) {
        const Square square = static_cast<Square>(std::countr_zero(remaining));
        remaining &= remaining - 1;
        const int file = squareIndex(square) % 8;
        const int rank = squareIndex(square) / 8;
        const bool hasLeft = file > 0 && fileCounts[static_cast<std::size_t>(file - 1)] > 0;
        const bool hasRight = file < 7 && fileCounts[static_cast<std::size_t>(file + 1)] > 0;
        if (!hasLeft && !hasRight) add(result.pawnStructure, {-12, -10}, multiplier);

        bool connected = false;
        for (const int adjacentFile : {file - 1, file + 1}) {
            if (adjacentFile < 0 || adjacentFile >= 8) continue;
            for (const int rankDelta : {-1, 0, 1}) {
                const int adjacentRank = rank + rankDelta;
                if (adjacentRank < 0 || adjacentRank >= 8) continue;
                const Square candidate = static_cast<Square>(adjacentRank * 8 + adjacentFile);
                if (testSquare(pawns, candidate)) connected = true;
            }
        }
        if (connected) add(result.pawnStructure, {7, 10}, multiplier);

        if (isPassedPawn(position, color, square)) {
            const int advance = relativeRank(color, square);
            add(result.pawnStructure,
                {12 + advance * advance * 2, 20 + advance * advance * 5}, multiplier);
        }
    }
}

void evaluateRooks(const Position& position, Color color,
                   EvaluationBreakdown& result) noexcept {
    const int multiplier = sign(color);
    const Bitboard friendlyPawns = position.pieces(color, PieceType::Pawn);
    const Bitboard allPawns = position.pieces(PieceType::Pawn);
    Bitboard rooks = position.pieces(color, PieceType::Rook);
    while (rooks != 0) {
        const Square square = static_cast<Square>(std::countr_zero(rooks));
        rooks &= rooks - 1;
        const int file = squareIndex(square) % 8;
        const Bitboard fileMask = 0x0101010101010101ULL << file;
        if ((allPawns & fileMask) == 0) add(result.rookActivity, {20, 12}, multiplier);
        else if ((friendlyPawns & fileMask) == 0) add(result.rookActivity, {11, 7}, multiplier);
        if (relativeRank(color, square) == 6) add(result.rookActivity, {18, 28}, multiplier);
    }
}

void evaluateKingSafety(const Position& position, Color color,
                        Bitboard enemyAttacks,
                        EvaluationBreakdown& result) {
    const int multiplier = sign(color);
    const Square king = position.kingSquare(color);
    const int file = squareIndex(king) % 8;
    const int rank = squareIndex(king) / 8;
    const int forward = color == Color::White ? 1 : -1;
    int shield = 0;
    for (int fileDelta = -1; fileDelta <= 1; ++fileDelta) {
        const int shieldFile = file + fileDelta;
        const int shieldRank = rank + forward;
        if (shieldFile < 0 || shieldFile >= 8 || shieldRank < 0 || shieldRank >= 8) continue;
        const Square square = static_cast<Square>(shieldRank * 8 + shieldFile);
        if (testSquare(position.pieces(color, PieceType::Pawn), square)) ++shield;
    }
    const int ringAttacks = countBits(kingAttacks(king) & enemyAttacks);
    add(result.kingSafety, {shield * 13 - ringAttacks * 16,
                            shield * 2 - ringAttacks * 3}, multiplier);
}

int taper(EvalTerm score, int phase) noexcept {
    return (score.middlegame * phase +
            score.endgame * (maxPhase - phase)) / maxPhase;
}

}  // namespace

EvaluationBreakdown evaluateDetailed(const Position& position) {
    if (countBits(position.pieces(Color::White, PieceType::King)) != 1 ||
        countBits(position.pieces(Color::Black, PieceType::King)) != 1) {
        throw std::invalid_argument("evaluation requires exactly one king per side");
    }

    EvaluationBreakdown result;
    for (const Color color : {Color::White, Color::Black}) {
        const int multiplier = sign(color);
        for (const PieceType type : {PieceType::Pawn, PieceType::Knight,
                                     PieceType::Bishop, PieceType::Rook,
                                     PieceType::Queen, PieceType::King}) {
            Bitboard pieces = position.pieces(color, type);
            const int count = countBits(pieces);
            add(result.material, materialValues[static_cast<std::size_t>(type)],
                multiplier * count);
            result.phase += count * phaseWeights[static_cast<std::size_t>(type)];
            while (pieces != 0) {
                const Square square = static_cast<Square>(std::countr_zero(pieces));
                pieces &= pieces - 1;
                add(result.pieceSquare, pieceSquareValue(type, color, square), multiplier);
                if (type != PieceType::Pawn && type != PieceType::King) {
                    const int mobility = countBits(
                        attacksFrom(position, color, type, square) & ~position.pieces(color));
                    const int mgWeight = type == PieceType::Queen ? 2 : 4;
                    const int egWeight = type == PieceType::Rook ? 3 : 2;
                    add(result.mobility, {mobility * mgWeight, mobility * egWeight},
                        multiplier);
                }
            }
        }
        if (countBits(position.pieces(color, PieceType::Bishop)) >= 2) {
            add(result.bishopPair, {28, 42}, multiplier);
        }
        evaluatePawns(position, color, result);
        evaluateRooks(position, color, result);
    }

    result.phase = std::clamp(result.phase, 0, maxPhase);
    const Bitboard whiteAttacks = attackMap(position, Color::White);
    const Bitboard blackAttacks = attackMap(position, Color::Black);
    evaluateKingSafety(position, Color::White, blackAttacks, result);
    evaluateKingSafety(position, Color::Black, whiteAttacks, result);

    constexpr Bitboard whiteSpace = 0x0000FFFFFF000000ULL;
    constexpr Bitboard blackSpace = 0x000000FFFFFF0000ULL;
    add(result.space,
        {countBits(whiteAttacks & whiteSpace) * 3 -
         countBits(blackAttacks & blackSpace) * 3,
         countBits(whiteAttacks & whiteSpace) -
         countBits(blackAttacks & blackSpace)});

    EvalTerm total;
    for (const EvalTerm term : {result.material, result.pieceSquare, result.mobility,
                                result.pawnStructure, result.bishopPair,
                                result.rookActivity, result.kingSafety,
                                result.space}) {
        add(total, term);
    }
    result.whitePerspective = taper(total, result.phase);
    result.sideToMoveScore = position.sideToMove() == Color::White
        ? result.whitePerspective : -result.whitePerspective;
    return result;
}

int evaluate(const Position& position) {
    return evaluateDetailed(position).sideToMoveScore;
}

}  // namespace aggrochess
