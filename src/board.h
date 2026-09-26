#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace aggrochess {

using Bitboard = std::uint64_t;

// a1 is index 0/bit 0, b1 is 1, ..., h1 is 7, and h8 is 63.
enum class Square : std::uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};

constexpr int squareIndex(Square square) noexcept { return static_cast<int>(square); }
constexpr Bitboard squareMask(Square square) noexcept { return Bitboard{1} << squareIndex(square); }

void setSquare(Bitboard& bitboard, Square square) noexcept;
void clearSquare(Bitboard& bitboard, Square square) noexcept;
[[nodiscard]] bool testSquare(Bitboard bitboard, Square square) noexcept;
[[nodiscard]] Square popLeastSignificantBit(Bitboard& bitboard);
[[nodiscard]] int countBits(Bitboard bitboard) noexcept;

enum class Color : std::uint8_t { White, Black };
enum class PieceType : std::uint8_t { Pawn, Knight, Bishop, Rook, Queen, King, Count };

enum CastlingRights : std::uint8_t {
    NoCastling = 0, WhiteKingSide = 1U << 0, WhiteQueenSide = 1U << 1,
    BlackKingSide = 1U << 2, BlackQueenSide = 1U << 3
};

struct Undo;

enum class MoveType : std::uint8_t {
    Quiet,
    Capture,
    PawnDoublePush,
    EnPassant,
    Castle,
    Promotion,
    PromotionCapture
};

class Move {
public:
    constexpr Move(Square from, Square to, MoveType type,
                   PieceType promotion = PieceType::Queen) noexcept
        : from_(from), to_(to), type_(type), promotion_(promotion) {}

    [[nodiscard]] constexpr Square from() const noexcept { return from_; }
    [[nodiscard]] constexpr Square to() const noexcept { return to_; }
    [[nodiscard]] constexpr MoveType type() const noexcept { return type_; }
    [[nodiscard]] constexpr PieceType promotion() const noexcept { return promotion_; }

private:
    Square from_;
    Square to_;
    MoveType type_;
    PieceType promotion_;
};

class Position {
public:
    static Position startPosition();
    static Position fromFen(std::string_view fen);
    [[nodiscard]] std::string toFen() const;

    [[nodiscard]] Bitboard pieces(Color color) const noexcept;
    [[nodiscard]] Bitboard pieces(PieceType pieceType) const noexcept;
    [[nodiscard]] Bitboard pieces(Color color, PieceType pieceType) const noexcept;
    [[nodiscard]] Bitboard occupancy(Color color) const noexcept;
    [[nodiscard]] Bitboard allOccupancy() const noexcept;

    [[nodiscard]] Color sideToMove() const noexcept { return sideToMove_; }
    [[nodiscard]] std::uint8_t castlingRights() const noexcept { return castlingRights_; }
    [[nodiscard]] Square enPassantSquare() const noexcept { return enPassantSquare_; }
    [[nodiscard]] bool hasEnPassantSquare() const noexcept { return hasEnPassantSquare_; }
    [[nodiscard]] unsigned halfmoveClock() const noexcept { return halfmoveClock_; }
    [[nodiscard]] unsigned fullmoveNumber() const noexcept { return fullmoveNumber_; }
    [[nodiscard]] bool isValid() const noexcept;

private:
    friend void makeMove(Position&, const Move&, Undo&);
    friend void unmakeMove(Position&, const Undo&);
    void placePiece(Color color, PieceType pieceType, Square square) noexcept;
    void removePiece(Color color, PieceType pieceType, Square square) noexcept;
    std::array<Bitboard, 2> colorBitboards_{};
    std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)> pieceBitboards_{};
    Color sideToMove_ = Color::White;
    std::uint8_t castlingRights_ = NoCastling;
    Square enPassantSquare_ = Square::A1;
    bool hasEnPassantSquare_ = false;
    unsigned halfmoveClock_ = 0;
    unsigned fullmoveNumber_ = 1;
};

struct Undo {
    // The full snapshots are the restoration source of truth. Capture fields
    // are retained as useful move-history metadata for future search code.
    std::array<Bitboard, 2> colorBitboards{};
    std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)> pieceBitboards{};
    Color sideToMove = Color::White;
    std::uint8_t castlingRights = NoCastling;
    Square enPassantSquare = Square::A1;
    bool hasEnPassantSquare = false;
    unsigned halfmoveClock = 0;
    unsigned fullmoveNumber = 1;
    bool hasCapturedPiece = false;
    Color capturedColor = Color::White;
    PieceType capturedPiece = PieceType::Pawn;
    Square capturedSquare = Square::A1;
};

void makeMove(Position& position, const Move& move, Undo& undo);
void unmakeMove(Position& position, const Undo& undo);

[[nodiscard]] std::string squareToString(Square square);
[[nodiscard]] Square squareFromString(std::string_view value);

}  // namespace aggrochess