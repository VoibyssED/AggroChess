#include "board.h"

#include <bit>
#include <charconv>
#include <stdexcept>
#include <string>
#include <vector>

namespace aggrochess {
namespace {
constexpr std::size_t colorIndex(Color color) noexcept { return static_cast<std::size_t>(color); }
constexpr std::size_t pieceIndex(PieceType type) noexcept { return static_cast<std::size_t>(type); }

PieceType parsePiece(char symbol) {
    switch (static_cast<char>(symbol | 0x20)) {
    case 'p': return PieceType::Pawn;
    case 'n': return PieceType::Knight;
    case 'b': return PieceType::Bishop;
    case 'r': return PieceType::Rook;
    case 'q': return PieceType::Queen;
    case 'k': return PieceType::King;
    default: throw std::invalid_argument("FEN contains an invalid piece");
    }
}

char pieceSymbol(PieceType type, Color color) {
    constexpr char symbols[] = {'p', 'n', 'b', 'r', 'q', 'k'};
    const char symbol = symbols[pieceIndex(type)];
    return color == Color::White ? static_cast<char>(symbol - ('a' - 'A')) : symbol;
}

unsigned parseUnsigned(std::string_view value, const char* name) {
    if (value.empty()) throw std::invalid_argument(std::string("empty FEN ") + name);
    unsigned result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument(std::string("invalid FEN ") + name);
    }
    return result;
}

std::vector<std::string> splitFields(std::string_view fen) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start < fen.size()) {
        while (start < fen.size() && fen[start] == ' ') ++start;
        if (start == fen.size()) break;
        const std::size_t end = fen.find(' ', start);
        fields.emplace_back(fen.substr(start, end == std::string_view::npos
                                                 ? fen.size() - start : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return fields;
}
}  // namespace

void setSquare(Bitboard& bitboard, Square square) noexcept { bitboard |= squareMask(square); }
void clearSquare(Bitboard& bitboard, Square square) noexcept { bitboard &= ~squareMask(square); }
bool testSquare(Bitboard bitboard, Square square) noexcept { return (bitboard & squareMask(square)) != 0; }

Square popLeastSignificantBit(Bitboard& bitboard) {
    if (bitboard == 0) throw std::invalid_argument("cannot pop an empty bitboard");
    const auto square = static_cast<Square>(std::countr_zero(bitboard));
    bitboard &= bitboard - 1;
    return square;
}

int countBits(Bitboard bitboard) noexcept { return std::popcount(bitboard); }

std::string squareToString(Square square) {
    const int index = squareIndex(square);
    if (index < 0 || index >= 64) throw std::invalid_argument("invalid square");
    return {static_cast<char>('a' + index % 8), static_cast<char>('1' + index / 8)};
}

Square squareFromString(std::string_view value) {
    if (value.size() != 2 || value[0] < 'a' || value[0] > 'h' ||
        value[1] < '1' || value[1] > '8') throw std::invalid_argument("invalid square");
    return static_cast<Square>((value[1] - '1') * 8 + value[0] - 'a');
}

void Position::placePiece(Color color, PieceType type, Square square) noexcept {
    setSquare(colorBitboards_[colorIndex(color)], square);
    setSquare(pieceBitboards_[pieceIndex(type)], square);
}

void Position::removePiece(Color color, PieceType type, Square square) noexcept {
    clearSquare(colorBitboards_[colorIndex(color)], square);
    clearSquare(pieceBitboards_[pieceIndex(type)], square);
}

Position Position::startPosition() {
    return fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

Position Position::fromFen(std::string_view fen) {
    const auto fields = splitFields(fen);
    if (fields.size() != 6) throw std::invalid_argument("FEN must have six fields");
    Position position;

    int rank = 7;
    int file = 0;
    for (const char symbol : fields[0]) {
        if (symbol == '/') {
            if (file != 8 || rank == 0) throw std::invalid_argument("invalid FEN board");
            --rank;
            file = 0;
        } else if (file >= 8) {
            throw std::invalid_argument("FEN rank is too long");
        } else if (symbol >= '1' && symbol <= '8') {
            file += symbol - '0';
        } else {
            const auto type = parsePiece(symbol);
            const auto color = symbol >= 'A' && symbol <= 'Z' ? Color::White : Color::Black;
            position.placePiece(color, type, static_cast<Square>(rank * 8 + file));
            ++file;
        }
    }
    if (rank != 0 || file != 8) throw std::invalid_argument("FEN must have eight complete ranks");

    if (fields[1] == "w") position.sideToMove_ = Color::White;
    else if (fields[1] == "b") position.sideToMove_ = Color::Black;
    else throw std::invalid_argument("invalid FEN side");

    if (fields[2] != "-") {
        if (fields[2].empty() || fields[2].size() > 4 ||
            fields[2].find_first_not_of("KQkq") != std::string::npos) {
            throw std::invalid_argument("invalid FEN castling rights");
        }
        for (const char right : fields[2]) {
            const auto bit = right == 'K' ? WhiteKingSide : right == 'Q' ? WhiteQueenSide :
                             right == 'k' ? BlackKingSide : BlackQueenSide;
            if ((position.castlingRights_ & bit) != 0) throw std::invalid_argument("duplicate castling right");
            position.castlingRights_ |= bit;
        }
    }

    if (fields[3] != "-") {
        position.enPassantSquare_ = squareFromString(fields[3]);
        const int epRank = squareIndex(position.enPassantSquare_) / 8;
        if (epRank != 2 && epRank != 5) throw std::invalid_argument("invalid FEN en-passant rank");
        position.hasEnPassantSquare_ = true;
    }

    position.halfmoveClock_ = parseUnsigned(fields[4], "halfmove clock");
    position.fullmoveNumber_ = parseUnsigned(fields[5], "fullmove number");
    if (position.fullmoveNumber_ == 0) throw std::invalid_argument("FEN fullmove number must be positive");
    return position;
}

Bitboard Position::pieces(Color color) const noexcept { return colorBitboards_[colorIndex(color)]; }
Bitboard Position::pieces(PieceType type) const noexcept { return pieceBitboards_[pieceIndex(type)]; }
Bitboard Position::pieces(Color color, PieceType type) const noexcept { return pieces(color) & pieces(type); }
Bitboard Position::occupancy(Color color) const noexcept { return pieces(color); }
Bitboard Position::allOccupancy() const noexcept { return pieces(Color::White) | pieces(Color::Black); }

bool Position::isValid() const noexcept {
    if ((castlingRights_ & static_cast<std::uint8_t>(~0x0FU)) != 0) return false;
    if ((pieces(Color::White) & pieces(Color::Black)) != 0) return false;
    Bitboard unionOfTypes = 0;
    for (const auto type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                            PieceType::Rook, PieceType::Queen, PieceType::King}) {
        if ((unionOfTypes & pieces(type)) != 0) return false;
        unionOfTypes |= pieces(type);
    }
    if (unionOfTypes != allOccupancy()) return false;
    for (const auto color : {Color::White, Color::Black}) {
        Bitboard colorPieces = 0;
        for (const auto type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                PieceType::Rook, PieceType::Queen, PieceType::King}) {
            const Bitboard intersection = pieces(color, type);
            if ((intersection & ~pieces(color)) != 0 ||
                (colorPieces & intersection) != 0) return false;
            colorPieces |= intersection;
        }
        if (colorPieces != pieces(color)) return false;
    }
    return true;
}

namespace {

PieceType pieceAt(const Position& position, Color color, Square square) {
    for (const auto type : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                            PieceType::Rook, PieceType::Queen, PieceType::King}) {
        if (testSquare(position.pieces(color, type), square)) return type;
    }
    throw std::invalid_argument("move source or capture square has no piece");
}

bool hasPiece(const Position& position, Color color, Square square) {
    return testSquare(position.pieces(color), square);
}

void updateCastlingRights(std::uint8_t& rights, Color color, PieceType movedPiece,
                          Square from, const Undo& undo) {
    if (movedPiece == PieceType::King) {
        rights &= color == Color::White
            ? static_cast<std::uint8_t>(~(WhiteKingSide | WhiteQueenSide))
            : static_cast<std::uint8_t>(~(BlackKingSide | BlackQueenSide));
    } else if (movedPiece == PieceType::Rook) {
        if (from == Square::A1) rights &= ~WhiteQueenSide;
        if (from == Square::H1) rights &= ~WhiteKingSide;
        if (from == Square::A8) rights &= ~BlackQueenSide;
        if (from == Square::H8) rights &= ~BlackKingSide;
    }
    if (undo.hasCapturedPiece && undo.capturedPiece == PieceType::Rook) {
        if (undo.capturedSquare == Square::A1) rights &= ~WhiteQueenSide;
        if (undo.capturedSquare == Square::H1) rights &= ~WhiteKingSide;
        if (undo.capturedSquare == Square::A8) rights &= ~BlackQueenSide;
        if (undo.capturedSquare == Square::H8) rights &= ~BlackKingSide;
    }
}

}  // namespace

void makeMove(Position& position, const Move& move, Undo& undo) {
    if (!position.isValid()) throw std::invalid_argument("cannot move an invalid position");

    const Color movingColor = position.sideToMove_;
    const Color enemyColor = movingColor == Color::White ? Color::Black : Color::White;
    if (!hasPiece(position, movingColor, move.from())) {
        throw std::invalid_argument("move source does not contain a friendly piece: " +
                                    squareToString(move.from()));
    }
    const PieceType movedPiece = pieceAt(position, movingColor, move.from());
    if (hasPiece(position, movingColor, move.to())) {
        throw std::invalid_argument("move destination contains a friendly piece");
    }

    const bool promotion = move.type() == MoveType::Promotion ||
                           move.type() == MoveType::PromotionCapture;
    const bool capture = move.type() == MoveType::Capture ||
                         move.type() == MoveType::PromotionCapture ||
                         move.type() == MoveType::EnPassant;
    const bool validPromotionPiece =
        move.promotion() == PieceType::Knight || move.promotion() == PieceType::Bishop ||
        move.promotion() == PieceType::Rook || move.promotion() == PieceType::Queen;
    if (promotion && (movedPiece != PieceType::Pawn || !validPromotionPiece)) {
        throw std::invalid_argument("invalid promotion move");
    }
    if (move.type() == MoveType::PawnDoublePush && movedPiece != PieceType::Pawn) {
        throw std::invalid_argument("pawn double push requires a pawn");
    }
    if (move.type() == MoveType::Castle) {
        if (movedPiece != PieceType::King || hasPiece(position, enemyColor, move.to())) {
            throw std::invalid_argument("invalid castling move");
        }
        const bool kingSide = squareIndex(move.to()) > squareIndex(move.from());
        const Square rookFrom = movingColor == Color::White
            ? (kingSide ? Square::H1 : Square::A1)
            : (kingSide ? Square::H8 : Square::A8);
        if (!hasPiece(position, movingColor, rookFrom) ||
            pieceAt(position, movingColor, rookFrom) != PieceType::Rook) {
            throw std::invalid_argument("castling move has no rook");
        }
    }

    if (!capture && hasPiece(position, enemyColor, move.to())) {
        throw std::invalid_argument("non-capture move lands on an enemy piece");
    }
    Square captureSquare = move.to();
    if (move.type() == MoveType::EnPassant) {
        if (hasPiece(position, enemyColor, move.to())) {
            throw std::invalid_argument("en-passant destination must be empty");
        }
        const int delta = movingColor == Color::White ? -8 : 8;
        const int captureIndex = squareIndex(move.to()) + delta;
        if (captureIndex < 0 || captureIndex >= 64) {
            throw std::invalid_argument("en-passant capture square is invalid");
        }
        captureSquare = static_cast<Square>(captureIndex);
        if (!hasPiece(position, enemyColor, captureSquare) ||
            pieceAt(position, enemyColor, captureSquare) != PieceType::Pawn) {
            throw std::invalid_argument("en-passant requires an enemy pawn behind the destination");
        }
    } else if (capture && !hasPiece(position, enemyColor, move.to())) {
        throw std::invalid_argument("capture move has no enemy piece at " +
                                    squareToString(move.to()));
    }

    // All checks above are read-only. Only now snapshot the state and mutate it.
    undo.colorBitboards = position.colorBitboards_;
    undo.pieceBitboards = position.pieceBitboards_;
    undo.sideToMove = position.sideToMove_;
    undo.castlingRights = position.castlingRights_;
    undo.enPassantSquare = position.enPassantSquare_;
    undo.hasEnPassantSquare = position.hasEnPassantSquare_;
    undo.halfmoveClock = position.halfmoveClock_;
    undo.fullmoveNumber = position.fullmoveNumber_;
    undo.hasCapturedPiece = false;

    if (capture) {
        undo.hasCapturedPiece = true;
        undo.capturedColor = enemyColor;
        undo.capturedSquare = captureSquare;
        undo.capturedPiece = pieceAt(position, enemyColor, captureSquare);
        position.removePiece(enemyColor, undo.capturedPiece, captureSquare);
    }

    position.removePiece(movingColor, movedPiece, move.from());
    if (move.type() == MoveType::Promotion || move.type() == MoveType::PromotionCapture) {
        position.placePiece(movingColor, move.promotion(), move.to());
    } else {
        position.placePiece(movingColor, movedPiece, move.to());
    }

    if (move.type() == MoveType::Castle) {
        const bool kingSide = squareIndex(move.to()) > squareIndex(move.from());
        const Square rookFrom = movingColor == Color::White
            ? (kingSide ? Square::H1 : Square::A1)
            : (kingSide ? Square::H8 : Square::A8);
        const Square rookTo = movingColor == Color::White
            ? (kingSide ? Square::F1 : Square::D1)
            : (kingSide ? Square::F8 : Square::D8);
        position.removePiece(movingColor, PieceType::Rook, rookFrom);
        position.placePiece(movingColor, PieceType::Rook, rookTo);
    }

    updateCastlingRights(position.castlingRights_, movingColor, movedPiece, move.from(), undo);
    position.hasEnPassantSquare_ = false;
    if (move.type() == MoveType::PawnDoublePush) {
        const int delta = movingColor == Color::White ? 8 : -8;
        position.enPassantSquare_ = static_cast<Square>(squareIndex(move.from()) + delta);
        position.hasEnPassantSquare_ = true;
    }
    position.halfmoveClock_ = movedPiece == PieceType::Pawn || capture
        ? 0 : position.halfmoveClock_ + 1;
    if (movingColor == Color::Black) ++position.fullmoveNumber_;
    position.sideToMove_ = enemyColor;
}

void unmakeMove(Position& position, const Undo& undo) {
    position.colorBitboards_ = undo.colorBitboards;
    position.pieceBitboards_ = undo.pieceBitboards;
    position.sideToMove_ = undo.sideToMove;
    position.castlingRights_ = undo.castlingRights;
    position.enPassantSquare_ = undo.enPassantSquare;
    position.hasEnPassantSquare_ = undo.hasEnPassantSquare;
    position.halfmoveClock_ = undo.halfmoveClock;
    position.fullmoveNumber_ = undo.fullmoveNumber;
}

std::string Position::toFen() const {
    std::string result;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const auto square = static_cast<Square>(rank * 8 + file);
            PieceType type = PieceType::Count;
            Color color = Color::White;
            for (const auto candidateColor : {Color::White, Color::Black}) {
                for (int index = 0; index < static_cast<int>(PieceType::Count); ++index) {
                    const auto candidate = static_cast<PieceType>(index);
                    if (testSquare(pieces(candidateColor, candidate), square)) {
                        type = candidate;
                        color = candidateColor;
                    }
                }
            }
            if (type == PieceType::Count) ++empty;
            else {
                if (empty != 0) result += std::to_string(empty);
                empty = 0;
                result += pieceSymbol(type, color);
            }
        }
        if (empty != 0) result += std::to_string(empty);
        if (rank != 0) result += '/';
    }
    result += sideToMove_ == Color::White ? " w " : " b ";
    if (castlingRights_ == NoCastling) result += '-';
    else {
        if (castlingRights_ & WhiteKingSide) result += 'K';
        if (castlingRights_ & WhiteQueenSide) result += 'Q';
        if (castlingRights_ & BlackKingSide) result += 'k';
        if (castlingRights_ & BlackQueenSide) result += 'q';
    }
    result += ' ';
    result += hasEnPassantSquare_ ? squareToString(enPassantSquare_) : "-";
    result += ' ' + std::to_string(halfmoveClock_);
    result += ' ' + std::to_string(fullmoveNumber_);
    return result;
}

}  // namespace aggrochess