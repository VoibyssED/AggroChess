#pragma once

#include "game_state.h"
#include "transposition_table.h"

#include <iosfwd>
#include <string_view>

namespace aggrochess {

class Engine {
public:
    Engine();

    void newGame();
    void setPosition(std::string_view command);
    void setHashSize(std::size_t megabytes);

    [[nodiscard]] Move chooseMove(int* score = nullptr) const;
    [[nodiscard]] const Position& position() const noexcept { return position_; }

private:
    Position position_;
    PositionHistory history_;
    TranspositionTable table_;
};

void runUci(std::istream& input, std::ostream& output);

}  // namespace aggrochess
