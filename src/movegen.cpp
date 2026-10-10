#include "movegen.h"

#include "attacks.h"

#include <array>
#include <initializer_list>
#include <stdexcept>

namespace aggrochess {
namespace {

constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black : Color::White;
}

constexpr Square squareAt(int index) noexcept {
    return static_cast<Square>(index);
}

constexpr char promotionSymbol(PieceType type) noexcept {
    switch (type) {
    case PieceType::Knight: return 'n';
    case PieceType::Bishop: return 'b';
    case PieceType::Rook: return 'r';
    case PieceType::Queen: return 'q';
    case PieceType::Pawn:
    case PieceType::King:
    case PieceType::Count: return '\0';
    }
    return '\0';
}

void addPromotions(MoveList& moves, Square from, Square to, bool capture) {
    constexpr std::array<PieceType, 4> promotionPieces = {
        PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen
    };
    const MoveType type = capture ? MoveType::PromotionCapture : MoveType::Promotion;
    for (const PieceType promotion : promotionPieces) {
        moves.emplace_back(from, to, type, promotion);
    }
}

void addPawnMoves(const Position& position, MoveList& moves, Color color,
                  Bitboard enemyCapturable) {
    const Bitboard occupancy = position.allOccupancy();
    const Color enemy = opposite(color);
    const int step = color == Color::White ? 8 : -8;
    const int startRank = color == Color::White ? 1 : 6;
    const int promotionRank = color == Color::White ? 7 : 0;

    Bitboard pawns = position.pieces(color, PieceType::Pawn);
    while (pawns != 0) {
        const Square from = popLeastSignificantBit(pawns);
        const int fromIndex = squareIndex(from);
        const int fromRank = fromIndex / 8;
        const int oneStepIndex = fromIndex + step;

        if (oneStepIndex >= 0 && oneStepIndex < 64) {
            const Square oneStep = squareAt(oneStepIndex);
            if (!testSquare(occupancy, oneStep)) {
                if (oneStepIndex / 8 == promotionRank) {
                    addPromotions(moves, from, oneStep, false);
                } else {
                    moves.emplace_back(from, oneStep, MoveType::Quiet);
                    if (fromRank == startRank) {
                        const Square twoSteps = squareAt(fromIndex + 2 * step);
                        if (!testSquare(occupancy, twoSteps)) {
                            moves.emplace_back(from, twoSteps,
                                               MoveType::PawnDoublePush);
                        }
                    }
                }
            }
        }

        Bitboard captures = pawnAttacks(color, from) & enemyCapturable;
        while (captures != 0) {
            const Square to = popLeastSignificantBit(captures);
            if (squareIndex(to) / 8 == promotionRank) {
                addPromotions(moves, from, to, true);
            } else {
                moves.emplace_back(from, to, MoveType::Capture);
            }
        }

        if (!position.hasEnPassantSquare()) continue;
        const Square target = position.enPassantSquare();
        if (!testSquare(pawnAttacks(color, from), target) ||
            testSquare(occupancy, target)) {
            continue;
        }
        const int capturedIndex = squareIndex(target) - step;
        if (capturedIndex < 0 || capturedIndex >= 64) continue;
        const Square capturedSquare = squareAt(capturedIndex);
        if (testSquare(position.pieces(enemy, PieceType::Pawn), capturedSquare)) {
            moves.emplace_back(from, target, MoveType::EnPassant);
        }
    }
}

void addPieceMoves(const Position& position, MoveList& moves, Color color,
                   PieceType type, Bitboard enemyCapturable) {
    const Bitboard friendly = position.pieces(color);
    const Bitboard occupancy = position.allOccupancy();
    Bitboard pieces = position.pieces(color, type);
    while (pieces != 0) {
        const Square from = popLeastSignificantBit(pieces);
        Bitboard targets = 0;
        switch (type) {
        case PieceType::Knight:
            targets = knightAttacks(from);
            break;
        case PieceType::Bishop:
            targets = bishopAttacks(from, occupancy);
            break;
        case PieceType::Rook:
            targets = rookAttacks(from, occupancy);
            break;
        case PieceType::Queen:
            targets = queenAttacks(from, occupancy);
            break;
        case PieceType::King:
            targets = kingAttacks(from);
            break;
        case PieceType::Pawn:
        case PieceType::Count:
            throw std::logic_error("invalid piece type for generic move generation");
        }
        targets &= ~friendly;
        // Kings are never captured; checkmate is represented by no legal reply.
        targets &= ~position.pieces(opposite(color), PieceType::King);

        while (targets != 0) {
            const Square to = popLeastSignificantBit(targets);
            const MoveType moveType = testSquare(enemyCapturable, to)
                ? MoveType::Capture : MoveType::Quiet;
            moves.emplace_back(from, to, moveType);
        }
    }
}

bool squaresEmpty(const Position& position,
                  std::initializer_list<Square> squares) noexcept {
    const Bitboard occupancy = position.allOccupancy();
    for (const Square square : squares) {
        if (testSquare(occupancy, square)) return false;
    }
    return true;
}

bool squaresSafe(const Position& position, Color enemy,
                 std::initializer_list<Square> squares) noexcept {
    for (const Square square : squares) {
        if (isSquareAttacked(position, square, enemy)) return false;
    }
    return true;
}

void addCastlingMoves(const Position& position, MoveList& moves, Color color) {
    const Color enemy = opposite(color);
    const std::uint8_t rights = position.castlingRights();
    if (color == Color::White) {
        if (!testSquare(position.pieces(Color::White, PieceType::King), Square::E1)) {
            return;
        }
        if ((rights & WhiteKingSide) != 0 &&
            testSquare(position.pieces(Color::White, PieceType::Rook), Square::H1) &&
            squaresEmpty(position, {Square::F1, Square::G1}) &&
            squaresSafe(position, enemy, {Square::E1, Square::F1, Square::G1})) {
            moves.emplace_back(Square::E1, Square::G1, MoveType::Castle);
        }
        if ((rights & WhiteQueenSide) != 0 &&
            testSquare(position.pieces(Color::White, PieceType::Rook), Square::A1) &&
            squaresEmpty(position, {Square::B1, Square::C1, Square::D1}) &&
            squaresSafe(position, enemy, {Square::E1, Square::D1, Square::C1})) {
            moves.emplace_back(Square::E1, Square::C1, MoveType::Castle);
        }
    } else {
        if (!testSquare(position.pieces(Color::Black, PieceType::King), Square::E8)) {
            return;
        }
        if ((rights & BlackKingSide) != 0 &&
            testSquare(position.pieces(Color::Black, PieceType::Rook), Square::H8) &&
            squaresEmpty(position, {Square::F8, Square::G8}) &&
            squaresSafe(position, enemy, {Square::E8, Square::F8, Square::G8})) {
            moves.emplace_back(Square::E8, Square::G8, MoveType::Castle);
        }
        if ((rights & BlackQueenSide) != 0 &&
            testSquare(position.pieces(Color::Black, PieceType::Rook), Square::A8) &&
            squaresEmpty(position, {Square::B8, Square::C8, Square::D8}) &&
            squaresSafe(position, enemy, {Square::E8, Square::D8, Square::C8})) {
            moves.emplace_back(Square::E8, Square::C8, MoveType::Castle);
        }
    }
}

MoveList generatePseudoLegalMoves(const Position& position) {
    const Color color = position.sideToMove();
    const Color enemy = opposite(color);
    if (countBits(position.pieces(color, PieceType::King)) != 1 ||
        countBits(position.pieces(enemy, PieceType::King)) != 1) {
        throw std::invalid_argument("move generation requires exactly one king per side");
    }

    MoveList moves;
    moves.reserve(64);
    const Bitboard enemyCapturable =
        position.pieces(enemy) & ~position.pieces(enemy, PieceType::King);

    addPawnMoves(position, moves, color, enemyCapturable);
    addPieceMoves(position, moves, color, PieceType::Knight, enemyCapturable);
    addPieceMoves(position, moves, color, PieceType::Bishop, enemyCapturable);
    addPieceMoves(position, moves, color, PieceType::Rook, enemyCapturable);
    addPieceMoves(position, moves, color, PieceType::Queen, enemyCapturable);
    addPieceMoves(position, moves, color, PieceType::King, enemyCapturable);
    addCastlingMoves(position, moves, color);
    return moves;
}

}  // namespace

MoveList generateLegalMoves(const Position& position) {
    const Color movingColor = position.sideToMove();
    const MoveList pseudoLegal = generatePseudoLegalMoves(position);
    MoveList legal;
    legal.reserve(pseudoLegal.size());

    Position working = position;
    for (const Move& move : pseudoLegal) {
        Undo undo;
        makeMove(working, move, undo);
        if (!isInCheck(working, movingColor)) legal.push_back(move);
        unmakeMove(working, undo);
    }
    return legal;
}

std::string moveToUci(const Move& move) {
    if (!move.isValid()) throw std::invalid_argument("cannot serialize an invalid move");
    if (move.isNull()) return "0000";

    std::string result = squareToString(move.from()) + squareToString(move.to());
    if (move.type() == MoveType::Promotion ||
        move.type() == MoveType::PromotionCapture) {
        result += promotionSymbol(move.promotion());
    }
    return result;
}

Move moveFromUci(const Position& position, std::string_view value) {
    if (value == "0000") {
        throw std::invalid_argument("a null move is not a legal position move");
    }
    if (value.size() != 4 && value.size() != 5) {
        throw std::invalid_argument("UCI move must contain four or five characters");
    }

    const Square from = squareFromString(value.substr(0, 2));
    const Square to = squareFromString(value.substr(2, 2));
    char requestedPromotion = '\0';
    if (value.size() == 5) {
        requestedPromotion = value[4];
        if (requestedPromotion != 'n' && requestedPromotion != 'b' &&
            requestedPromotion != 'r' && requestedPromotion != 'q') {
            throw std::invalid_argument("invalid UCI promotion piece");
        }
    }

    for (const Move& move : generateLegalMoves(position)) {
        if (move.from() != from || move.to() != to) continue;
        const bool promotion = move.type() == MoveType::Promotion ||
                               move.type() == MoveType::PromotionCapture;
        if ((!promotion && requestedPromotion == '\0') ||
            (promotion && promotionSymbol(move.promotion()) == requestedPromotion)) {
            return move;
        }
    }
    throw std::invalid_argument("UCI move is not legal in the position");
}

std::uint64_t perft(Position& position, unsigned depth) {
    if (depth == 0) return 1;
    const MoveList moves = generateLegalMoves(position);
    if (depth == 1) return static_cast<std::uint64_t>(moves.size());

    std::uint64_t nodes = 0;
    for (const Move& move : moves) {
        Undo undo;
        makeMove(position, move, undo);
        nodes += perft(position, depth - 1);
        unmakeMove(position, undo);
    }
    return nodes;
}

}  // namespace aggrochess
