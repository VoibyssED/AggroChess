#include "game_state.h"

#include "attacks.h"
#include "movegen.h"

#include <algorithm>
#include <stdexcept>

namespace aggrochess {

PositionHistory::PositionHistory(const Position& initialPosition) {
    reset(initialPosition);
}

void PositionHistory::reset(const Position& initialPosition) {
    keys_.clear();
    keys_.push_back(initialPosition.zobristKey());
}

void PositionHistory::clear() noexcept {
    keys_.clear();
}

void PositionHistory::push(const Position& position) {
    keys_.push_back(position.zobristKey());
}

void PositionHistory::pop() {
    if (keys_.empty()) throw std::logic_error("cannot pop empty position history");
    keys_.pop_back();
}

ZobristKey PositionHistory::currentKey() const {
    if (keys_.empty()) throw std::logic_error("empty position history has no current key");
    return keys_.back();
}

int PositionHistory::repetitionCount(const Position& position) const noexcept {
    if (keys_.empty() || keys_.back() != position.zobristKey()) return 0;

    int count = 1;
    const std::size_t current = keys_.size() - 1U;
    const std::size_t reversiblePlies = std::min<std::size_t>(
        position.halfmoveClock(), current);
    // The same side must be on move, so only positions an even number of plies
    // apart can match. The key also contains side-to-move as a second guard.
    for (std::size_t distance = 2; distance <= reversiblePlies; distance += 2) {
        if (keys_[current - distance] == position.zobristKey()) ++count;
    }
    return count;
}

bool PositionHistory::isThreefoldRepetition(const Position& position) const noexcept {
    return repetitionCount(position) >= 3;
}

bool isFiftyMoveDraw(const Position& position) noexcept {
    return position.halfmoveClock() >= 100;
}

bool isInsufficientMaterial(const Position& position) noexcept {
    if ((position.pieces(PieceType::Pawn) | position.pieces(PieceType::Rook) |
         position.pieces(PieceType::Queen)) != 0) {
        return false;
    }

    const int knights = countBits(position.pieces(PieceType::Knight));
    Bitboard bishops = position.pieces(PieceType::Bishop);
    const int bishopCount = countBits(bishops);
    if (knights == 0 && bishopCount == 0) return true;
    if (knights + bishopCount == 1) return true;
    if (knights != 0) return false;

    // With bishops only, the position is dead exactly for this conservative
    // material test when every bishop moves on the same square colour.
    bool sawLight = false;
    bool sawDark = false;
    while (bishops != 0) {
        const Square square = popLeastSignificantBit(bishops);
        const int file = squareIndex(square) % 8;
        const int rank = squareIndex(square) / 8;
        if ((file + rank) % 2 == 0) sawDark = true;
        else sawLight = true;
    }
    return !(sawLight && sawDark);
}

GameStatus classifyGameState(const Position& position,
                             const PositionHistory* history) {
    const MoveList legalMoves = generateLegalMoves(position);
    if (legalMoves.empty()) {
        return isInCheck(position, position.sideToMove())
            ? GameStatus::Checkmate : GameStatus::Stalemate;
    }
    if (isInsufficientMaterial(position)) {
        return GameStatus::DrawByInsufficientMaterial;
    }
    if (isFiftyMoveDraw(position)) {
        return GameStatus::DrawByFiftyMoveRule;
    }
    if (history != nullptr && history->isThreefoldRepetition(position)) {
        return GameStatus::DrawByThreefoldRepetition;
    }
    return GameStatus::Ongoing;
}

}  // namespace aggrochess
