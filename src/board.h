#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace aggrochess {

using Bitboard = std::uint64_t;
using ZobristKey = std::uint64_t;

// a1 is index 0/bit 0, b1 is 1, ..., h1 is 7, and h8 is 63.
enum class Square : std::uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    None
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
    PromotionCapture,
    Null,
    Invalid
};

class Move {
public:
    constexpr Move() noexcept = default;

    constexpr Move(Square from, Square to, MoveType type,
                   PieceType promotion = PieceType::Queen) noexcept {
        if (type == MoveType::Null) {
            type_ = MoveType::Null;
            return;
        }
        if (!validSquare(from) || !validSquare(to) || type == MoveType::Invalid) return;
        if ((type == MoveType::Promotion || type == MoveType::PromotionCapture) &&
            (promotion < PieceType::Knight || promotion > PieceType::Queen)) return;
        from_ = from;
        to_ = to;
        type_ = type;
        promotion_ = type == MoveType::Promotion || type == MoveType::PromotionCapture
            ? promotion : PieceType::Queen;
    }

    [[nodiscard]] static constexpr Move null() noexcept {
        return Move(Square::A1, Square::A1, MoveType::Null);
    }

    [[nodiscard]] static constexpr Move fromRaw(std::uint16_t raw) noexcept {
        const std::uint16_t flag = raw >> FlagShift;
        const Square from = static_cast<Square>(raw & SquareMask);
        const Square to = static_cast<Square>((raw >> 6U) & SquareMask);
        if (flag == NullFlag) return null();
        if (flag == QuietFlag) return Move(from, to, MoveType::Quiet);
        if (flag == CaptureFlag) return Move(from, to, MoveType::Capture);
        if (flag == DoublePushFlag) return Move(from, to, MoveType::PawnDoublePush);
        if (flag == EnPassantFlag) return Move(from, to, MoveType::EnPassant);
        if (flag == CastleFlag) return Move(from, to, MoveType::Castle);
        if (flag >= KnightPromotionFlag && flag <= QueenPromotionFlag) {
            return Move(from, to, MoveType::Promotion,
                        promotionFromOffset(flag - KnightPromotionFlag));
        }
        if (flag >= KnightPromotionCaptureFlag &&
            flag <= QueenPromotionCaptureFlag) {
            return Move(from, to, MoveType::PromotionCapture,
                        promotionFromOffset(flag - KnightPromotionCaptureFlag));
        }
        return Move{};
    }

    [[nodiscard]] constexpr std::uint16_t raw() const noexcept {
        const std::uint16_t flag = encodedFlag();
        if (flag == 0) return 0;
        if (flag == NullFlag) return static_cast<std::uint16_t>(flag << FlagShift);
        return static_cast<std::uint16_t>(squareIndex(from_)) |
               static_cast<std::uint16_t>(squareIndex(to_) << 6U) |
               static_cast<std::uint16_t>(flag << FlagShift);
    }
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return type_ != MoveType::Invalid;
    }
    [[nodiscard]] constexpr bool isNull() const noexcept {
        return type_ == MoveType::Null;
    }
    [[nodiscard]] constexpr Square from() const noexcept { return from_; }
    [[nodiscard]] constexpr Square to() const noexcept { return to_; }
    [[nodiscard]] constexpr MoveType type() const noexcept { return type_; }
    [[nodiscard]] constexpr PieceType promotion() const noexcept { return promotion_; }

    [[nodiscard]] friend constexpr bool operator==(Move, Move) noexcept = default;

private:
    static constexpr std::uint16_t SquareMask = 0x3FU;
    static constexpr unsigned FlagShift = 12U;
    static constexpr std::uint16_t NullFlag = 1U;
    static constexpr std::uint16_t QuietFlag = 2U;
    static constexpr std::uint16_t CaptureFlag = 3U;
    static constexpr std::uint16_t DoublePushFlag = 4U;
    static constexpr std::uint16_t EnPassantFlag = 5U;
    static constexpr std::uint16_t CastleFlag = 6U;
    static constexpr std::uint16_t KnightPromotionFlag = 7U;
    static constexpr std::uint16_t QueenPromotionFlag = 10U;
    static constexpr std::uint16_t KnightPromotionCaptureFlag = 11U;
    static constexpr std::uint16_t QueenPromotionCaptureFlag = 14U;

    [[nodiscard]] static constexpr bool validSquare(Square square) noexcept {
        return squareIndex(square) >= 0 && squareIndex(square) < 64;
    }
    [[nodiscard]] static constexpr PieceType promotionFromOffset(
        std::uint16_t offset) noexcept {
        return static_cast<PieceType>(
            static_cast<std::uint16_t>(PieceType::Knight) + offset);
    }
    [[nodiscard]] constexpr std::uint16_t encodedFlag() const noexcept {
        switch (type_) {
        case MoveType::Quiet: return QuietFlag;
        case MoveType::Capture: return CaptureFlag;
        case MoveType::PawnDoublePush: return DoublePushFlag;
        case MoveType::EnPassant: return EnPassantFlag;
        case MoveType::Castle: return CastleFlag;
        case MoveType::Null: return NullFlag;
        case MoveType::Promotion:
            return static_cast<std::uint16_t>(
                KnightPromotionFlag + promotionOffset());
        case MoveType::PromotionCapture:
            return static_cast<std::uint16_t>(
                KnightPromotionCaptureFlag + promotionOffset());
        case MoveType::Invalid: return 0;
        }
        return 0;
    }
    [[nodiscard]] constexpr std::uint16_t promotionOffset() const noexcept {
        return static_cast<std::uint16_t>(promotion_) -
               static_cast<std::uint16_t>(PieceType::Knight);
    }

    Square from_ = Square::A1;
    Square to_ = Square::A1;
    MoveType type_ = MoveType::Invalid;
    PieceType promotion_ = PieceType::Queen;
};

static_assert(sizeof(Move) == 4,
              "Move must remain a compact transposition-table value");

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
    [[nodiscard]] PieceType pieceTypeAt(Square square) const noexcept;
    [[nodiscard]] Color colorAt(Square square) const;
    [[nodiscard]] Square kingSquare(Color color) const;

    [[nodiscard]] Color sideToMove() const noexcept { return sideToMove_; }
    [[nodiscard]] std::uint8_t castlingRights() const noexcept { return castlingRights_; }
    [[nodiscard]] Square enPassantSquare() const noexcept { return enPassantSquare_; }
    [[nodiscard]] bool hasEnPassantSquare() const noexcept { return hasEnPassantSquare_; }
    [[nodiscard]] unsigned halfmoveClock() const noexcept { return halfmoveClock_; }
    [[nodiscard]] unsigned fullmoveNumber() const noexcept { return fullmoveNumber_; }
    [[nodiscard]] ZobristKey zobristKey() const noexcept { return zobristKey_; }
    [[nodiscard]] bool isInternallyConsistent() const noexcept;
    [[nodiscard]] bool isLegalPosition() const noexcept;
    // Compatibility alias for the original board-invariant API.
    [[nodiscard]] bool isValid() const noexcept { return isInternallyConsistent(); }

private:
    friend void makeMove(Position&, const Move&, Undo&);
    friend void unmakeMove(Position&, const Undo&);
    void placePiece(Color color, PieceType pieceType, Square square) noexcept;
    void removePiece(Color color, PieceType pieceType, Square square) noexcept;
    void refreshKingSquare(Color color) noexcept;
    std::array<Bitboard, 2> colorBitboards_{};
    std::array<Bitboard, static_cast<std::size_t>(PieceType::Count)> pieceBitboards_{};
    // Zero means empty; 1..6 are white pieces and 7..12 are black pieces.
    std::array<std::uint8_t, 64> pieceCodes_{};
    std::array<Square, 2> kingSquares_{Square::None, Square::None};
    Color sideToMove_ = Color::White;
    std::uint8_t castlingRights_ = NoCastling;
    Square enPassantSquare_ = Square::A1;
    bool hasEnPassantSquare_ = false;
    // Zero means no hash component; otherwise file + 1.
    std::uint8_t enPassantHashFile_ = 0;
    unsigned halfmoveClock_ = 0;
    unsigned fullmoveNumber_ = 1;
    ZobristKey zobristKey_ = 0;
};

struct Undo {
    // Only information that cannot be reconstructed from the post-move
    // position and the move is retained. Piece placement is reversed directly.
    ZobristKey zobristKey = 0;
    unsigned halfmoveClock = 0;
    Move move{Square::A1, Square::A1, MoveType::Quiet};
    std::uint8_t castlingRights = NoCastling;
    Square enPassantSquare = Square::A1;
    bool hasEnPassantSquare = false;
    std::uint8_t enPassantHashFile = 0;
    // PieceType::Count is the no-capture sentinel.
    PieceType capturedPiece = PieceType::Count;
    Square capturedSquare = Square::A1;
};

static_assert(sizeof(Undo) <= 24,
              "Undo must remain a compact search-stack record");

void makeMove(Position& position, const Move& move, Undo& undo);
void unmakeMove(Position& position, const Undo& undo);

[[nodiscard]] std::string squareToString(Square square);
[[nodiscard]] Square squareFromString(std::string_view value);

}  // namespace aggrochess