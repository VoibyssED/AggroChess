#include "see.h"

#include "attacks.h"

#include <array>
#include <bit>
#include <optional>
#include <stdexcept>

namespace aggrochess {
namespace {

constexpr std::size_t colorIndex(Color color) noexcept {
    return static_cast<std::size_t>(color);
}

constexpr std::size_t pieceIndex(PieceType type) noexcept {
    return static_cast<std::size_t>(type);
}

constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black : Color::White;
}

struct ExchangeState {
    std::array<std::array<Bitboard, 6>, 2> pieces{};
    std::array<Square, 2> kings{Square::None, Square::None};
    Bitboard occupancy = 0;
};

Bitboard pieces(const ExchangeState& state, Color color, PieceType type) noexcept {
    return state.pieces[colorIndex(color)][pieceIndex(type)];
}

void removePiece(ExchangeState& state, Color color, PieceType type,
                 Square square) noexcept {
    clearSquare(state.pieces[colorIndex(color)][pieceIndex(type)], square);
    clearSquare(state.occupancy, square);
    if (type == PieceType::King) state.kings[colorIndex(color)] = Square::None;
}

void addPiece(ExchangeState& state, Color color, PieceType type,
              Square square) noexcept {
    setSquare(state.pieces[colorIndex(color)][pieceIndex(type)], square);
    setSquare(state.occupancy, square);
    if (type == PieceType::King) state.kings[colorIndex(color)] = square;
}

bool isAttacked(const ExchangeState& state, Square square, Color byColor) noexcept {
    Bitboard pawns = pieces(state, byColor, PieceType::Pawn);
    while (pawns != 0) {
        const Square from = static_cast<Square>(std::countr_zero(pawns));
        pawns &= pawns - 1;
        if (testSquare(pawnAttacks(byColor, from), square)) return true;
    }
    if ((knightAttacks(square) & pieces(state, byColor, PieceType::Knight)) != 0) {
        return true;
    }
    if ((kingAttacks(square) & pieces(state, byColor, PieceType::King)) != 0) {
        return true;
    }
    if ((bishopAttacks(square, state.occupancy) &
         (pieces(state, byColor, PieceType::Bishop) |
          pieces(state, byColor, PieceType::Queen))) != 0) {
        return true;
    }
    return (rookAttacks(square, state.occupancy) &
            (pieces(state, byColor, PieceType::Rook) |
             pieces(state, byColor, PieceType::Queen))) != 0;
}

Bitboard attackers(const ExchangeState& state, Color color, PieceType type,
                   Square target) noexcept {
    switch (type) {
    case PieceType::Pawn:
        return pieces(state, color, type) & pawnAttacks(opposite(color), target);
    case PieceType::Knight:
        return pieces(state, color, type) & knightAttacks(target);
    case PieceType::Bishop:
        return pieces(state, color, type) & bishopAttacks(target, state.occupancy);
    case PieceType::Rook:
        return pieces(state, color, type) & rookAttacks(target, state.occupancy);
    case PieceType::Queen:
        return pieces(state, color, type) & queenAttacks(target, state.occupancy);
    case PieceType::King:
        return pieces(state, color, type) & kingAttacks(target);
    case PieceType::Count:
        return 0;
    }
    return 0;
}

struct Selection {
    ExchangeState state;
    PieceType resultingPiece = PieceType::Count;
    int promotionGain = 0;
};

std::optional<Selection> selectLegalAttacker(
    const ExchangeState& state, Color color, Square target, PieceType victim,
    PieceType attackerType, Square from) {
    const Color enemy = opposite(color);
    Selection selection{state, attackerType, 0};
    removePiece(selection.state, enemy, victim, target);
    removePiece(selection.state, color, attackerType, from);
    if (attackerType == PieceType::Pawn) {
        const int targetRank = squareIndex(target) / 8;
        if ((color == Color::White && targetRank == 7) ||
            (color == Color::Black && targetRank == 0)) {
            selection.resultingPiece = PieceType::Queen;
            selection.promotionGain =
                pieceValue(PieceType::Queen) - pieceValue(PieceType::Pawn);
        }
    }
    addPiece(selection.state, color, selection.resultingPiece, target);

    const Square king = selection.state.kings[colorIndex(color)];
    if (king == Square::None || isAttacked(selection.state, king, enemy)) {
        return std::nullopt;
    }
    return selection;
}

int bestResponse(const ExchangeState& state, Color color, Square target,
                 PieceType victim) {
    if (victim == PieceType::King) return 0;
    constexpr std::array<PieceType, 6> order = {
        PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
        PieceType::Rook, PieceType::Queen, PieceType::King
    };

    int best = 0;
    for (const PieceType attackerType : order) {
        Bitboard candidates = attackers(state, color, attackerType, target);
        while (candidates != 0) {
            const Square from = static_cast<Square>(std::countr_zero(candidates));
            candidates &= candidates - 1;
            const auto selection = selectLegalAttacker(
                state, color, target, victim, attackerType, from);
            if (!selection.has_value()) continue;
            const int gain = pieceValue(victim) + selection->promotionGain -
                bestResponse(selection->state, opposite(color), target,
                             selection->resultingPiece);
            if (gain > best) best = gain;
        }
    }
    return best;
}

ExchangeState makeExchangeState(const Position& position) {
    ExchangeState state;
    for (const Color color : {Color::White, Color::Black}) {
        for (const PieceType type : {PieceType::Pawn, PieceType::Knight,
                                     PieceType::Bishop, PieceType::Rook,
                                     PieceType::Queen, PieceType::King}) {
            state.pieces[colorIndex(color)][pieceIndex(type)] =
                position.pieces(color, type);
        }
        state.kings[colorIndex(color)] = position.kingSquare(color);
    }
    state.occupancy = position.allOccupancy();
    return state;
}

}  // namespace

int staticExchangeEvaluation(const Position& position, const Move& move) {
    if (!move.isValid() || move.isNull()) {
        throw std::invalid_argument("SEE requires a valid non-null move");
    }
    if (move.type() == MoveType::Castle) return 0;

    const Color movingColor = position.sideToMove();
    const Color enemyColor = opposite(movingColor);
    if (!testSquare(position.pieces(movingColor), move.from())) {
        throw std::invalid_argument("SEE move has no friendly source piece");
    }
    const PieceType movedPiece = position.pieceTypeAt(move.from());
    const bool capture = move.type() == MoveType::Capture ||
                         move.type() == MoveType::PromotionCapture ||
                         move.type() == MoveType::EnPassant;
    const bool promotion = move.type() == MoveType::Promotion ||
                           move.type() == MoveType::PromotionCapture;
    if (promotion) {
        const int promotionRank = movingColor == Color::White ? 7 : 0;
        if (movedPiece != PieceType::Pawn ||
            squareIndex(move.to()) / 8 != promotionRank) {
            throw std::invalid_argument("SEE has an invalid promotion move");
        }
    }

    Square capturedSquare = move.to();
    PieceType capturedPiece = PieceType::Count;
    if (move.type() == MoveType::EnPassant) {
        const int step = movingColor == Color::White ? 8 : -8;
        const int capturedIndex = squareIndex(move.to()) - step;
        if (movedPiece != PieceType::Pawn || !position.hasEnPassantSquare() ||
            move.to() != position.enPassantSquare() || capturedIndex < 0 ||
            capturedIndex >= 64 || testSquare(position.allOccupancy(), move.to())) {
            throw std::invalid_argument("SEE has an invalid en-passant target");
        }
        capturedSquare = static_cast<Square>(capturedIndex);
        capturedPiece = PieceType::Pawn;
        if (!testSquare(position.pieces(enemyColor, PieceType::Pawn),
                        capturedSquare)) {
            throw std::invalid_argument("SEE en-passant move has no captured pawn");
        }
    } else if (capture) {
        if (!testSquare(position.pieces(enemyColor), move.to())) {
            throw std::invalid_argument("SEE capture has no enemy target");
        }
        capturedPiece = position.pieceTypeAt(move.to());
        if (capturedPiece == PieceType::King) {
            throw std::invalid_argument("SEE cannot capture a king");
        }
    } else if (testSquare(position.allOccupancy(), move.to())) {
        throw std::invalid_argument("SEE non-capture has an occupied destination");
    }

    ExchangeState state = makeExchangeState(position);
    removePiece(state, movingColor, movedPiece, move.from());
    if (capture) removePiece(state, enemyColor, capturedPiece, capturedSquare);
    const PieceType resultingPiece = promotion ? move.promotion() : movedPiece;
    addPiece(state, movingColor, resultingPiece, move.to());

    const Square ownKing = state.kings[colorIndex(movingColor)];
    if (ownKing == Square::None || isAttacked(state, ownKing, enemyColor)) {
        throw std::invalid_argument("SEE move leaves the moving king in check");
    }

    int gain = pieceValue(capturedPiece);
    if (promotion) {
        gain += pieceValue(resultingPiece) - pieceValue(PieceType::Pawn);
    }
    return gain - bestResponse(state, enemyColor, move.to(), resultingPiece);
}

}  // namespace aggrochess
