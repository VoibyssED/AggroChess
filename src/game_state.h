#pragma once

#include "board.h"

#include <cstddef>
#include <vector>

namespace aggrochess {

enum class GameStatus {
    Ongoing,
    Checkmate,
    Stalemate,
    DrawByFiftyMoveRule,
    DrawByThreefoldRepetition,
    DrawByInsufficientMaterial
};

// Stores position identities in game order, including the current position.
// Search can push after makeMove and pop before/after unmakeMove.
class PositionHistory {
public:
    PositionHistory() = default;
    explicit PositionHistory(const Position& initialPosition);

    void reset(const Position& initialPosition);
    void clear() noexcept;
    void push(const Position& position);
    void pop();

    [[nodiscard]] bool empty() const noexcept { return keys_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return keys_.size(); }
    [[nodiscard]] ZobristKey currentKey() const;
    [[nodiscard]] int repetitionCount(const Position& position) const noexcept;
    [[nodiscard]] bool isThreefoldRepetition(const Position& position) const noexcept;

private:
    std::vector<ZobristKey> keys_;
};

[[nodiscard]] bool isFiftyMoveDraw(const Position& position) noexcept;
[[nodiscard]] bool isInsufficientMaterial(const Position& position) noexcept;
[[nodiscard]] GameStatus classifyGameState(
    const Position& position, const PositionHistory* history = nullptr);

}  // namespace aggrochess
