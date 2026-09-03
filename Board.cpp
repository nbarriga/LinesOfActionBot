#include "Board.h"
#include <cassert>

Board::Board()
    : pieces_{INITIAL_BLACK, INITIAL_WHITE}, side_to_move_(Color::BLACK) {}

Board::Board(uint64_t black, uint64_t white, Color to_move)
    : pieces_{black, white}, side_to_move_(to_move) {}

void Board::apply_move(const Move& move) {
    uint8_t from = move.from();
    uint8_t to = move.to();
    assert(from < 64 && to < 64);

    uint64_t from_mask = 1ULL << from;
    uint64_t to_mask = 1ULL << to;

    size_t active_idx = static_cast<size_t>(side_to_move_);
    size_t opp_idx = static_cast<size_t>(~side_to_move_);

    // Piece being moved must belong to active player
    assert((pieces_[active_idx] & from_mask) != 0);
    // Active player cannot land on their own piece
    assert((pieces_[active_idx] & to_mask) == 0);

    // Update active player's bitboard: clear from square and set to square
    pieces_[active_idx] ^= (from_mask | to_mask);

    // If opponent piece is on the target square, capture it
    pieces_[opp_idx] &= ~to_mask;

    // Toggle turn to next player
    side_to_move_ = ~side_to_move_;
}

uint64_t Board::pieces(Color c) const {
    return pieces_[static_cast<size_t>(c)];
}

uint64_t Board::black_pieces() const {
    return pieces_[static_cast<size_t>(Color::BLACK)];
}

uint64_t Board::white_pieces() const {
    return pieces_[static_cast<size_t>(Color::WHITE)];
}

uint64_t Board::all_pieces() const {
    return pieces_[0] | pieces_[1];
}

Color Board::turn() const {
    return side_to_move_;
}

void Board::set_turn(Color c) {
    side_to_move_ = c;
}

char Board::piece_at(uint8_t sq) const {
    assert(sq < 64);
    uint64_t mask = 1ULL << sq;
    if (pieces_[static_cast<size_t>(Color::BLACK)] & mask) return 'X';
    if (pieces_[static_cast<size_t>(Color::WHITE)] & mask) return 'O';
    return '-';
}

void Board::print(std::ostream& os) const {
    os << "  +-----------------+\n";
    for (int rank = 7; rank >= 0; --rank) {
        os << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            uint8_t sq = make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(rank));
            os << piece_at(sq) << ' ';
        }
        os << "|\n";
    }
    os << "  +-----------------+\n";
    os << "    a b c d e f g h\n";
    os << "Side to move: " << (side_to_move_ == Color::BLACK ? "Black" : "White") << "\n";
}

bool Board::operator==(const Board& other) const {
    return pieces_[0] == other.pieces_[0] &&
           pieces_[1] == other.pieces_[1] &&
           side_to_move_ == other.side_to_move_;
}

bool Board::operator!=(const Board& other) const {
    return !(*this == other);
}
