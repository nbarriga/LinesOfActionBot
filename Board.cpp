#include "Board.h"
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace {

struct Direction {
    int df;
    int dr;
};

constexpr Direction AXIS_DIRECTIONS[4][2] = {
    { { 1,  0}, {-1,  0} }, // 0: Horizontal (East, West)
    { { 0,  1}, { 0, -1} }, // 1: Vertical (North, South)
    { { 1,  1}, {-1, -1} }, // 2: Diagonal SW-NE (NE, SW)
    { { 1, -1}, {-1,  1} }  // 3: Anti-diagonal NW-SE (SE, NW)
};

struct MoveLookupTables {
    uint64_t line_masks[64][4];
    uint64_t between_masks[64][64];

    MoveLookupTables() {
        for (int sq = 0; sq < 64; ++sq) {
            int f = square_file(static_cast<uint8_t>(sq));
            int r = square_rank(static_cast<uint8_t>(sq));

            // 0: Horizontal (rank)
            uint64_t rank_mask = 0;
            for (int file = 0; file < 8; ++file) {
                rank_mask |= (1ULL << make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(r)));
            }
            line_masks[sq][0] = rank_mask;

            // 1: Vertical (file)
            uint64_t file_mask = 0;
            for (int rank = 0; rank < 8; ++rank) {
                file_mask |= (1ULL << make_square(static_cast<uint8_t>(f), static_cast<uint8_t>(rank)));
            }
            line_masks[sq][1] = file_mask;

            // 2: Diagonal SW-NE (r - f == const)
            uint64_t diag_mask = 0;
            for (int file = 0; file < 8; ++file) {
                int rank = r - f + file;
                if (rank >= 0 && rank < 8) {
                    diag_mask |= (1ULL << make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(rank)));
                }
            }
            line_masks[sq][2] = diag_mask;

            // 3: Anti-diagonal NW-SE (r + f == const)
            uint64_t anti_diag_mask = 0;
            for (int file = 0; file < 8; ++file) {
                int rank = r + f - file;
                if (rank >= 0 && rank < 8) {
                    anti_diag_mask |= (1ULL << make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(rank)));
                }
            }
            line_masks[sq][3] = anti_diag_mask;
        }

        // Precompute between_masks
        for (int sq1 = 0; sq1 < 64; ++sq1) {
            int f1 = square_file(static_cast<uint8_t>(sq1));
            int r1 = square_rank(static_cast<uint8_t>(sq1));
            for (int sq2 = 0; sq2 < 64; ++sq2) {
                between_masks[sq1][sq2] = 0ULL;
                if (sq1 == sq2) continue;

                int f2 = square_file(static_cast<uint8_t>(sq2));
                int r2 = square_rank(static_cast<uint8_t>(sq2));
                int df = f2 - f1;
                int dr = r2 - r1;

                bool aligned = false;
                if (dr == 0 && df != 0) aligned = true;
                else if (df == 0 && dr != 0) aligned = true;
                else if (std::abs(df) == std::abs(dr)) aligned = true;

                if (aligned) {
                    int step_f = (df > 0) ? 1 : ((df < 0) ? -1 : 0);
                    int step_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
                    int curr_f = f1 + step_f;
                    int curr_r = r1 + step_r;
                    uint64_t mask = 0ULL;
                    while (curr_f != f2 || curr_r != r2) {
                        mask |= (1ULL << make_square(static_cast<uint8_t>(curr_f), static_cast<uint8_t>(curr_r)));
                        curr_f += step_f;
                        curr_r += step_r;
                    }
                    between_masks[sq1][sq2] = mask;
                }
            }
        }
    }
};

const MoveLookupTables LOOKUP_TABLES;

} // anonymous namespace

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

void Board::generate_legal_moves(std::vector<Move>& moves) const {
    moves.clear();
    uint64_t friendly = pieces(side_to_move_);
    uint64_t opponent = pieces(~side_to_move_);
    uint64_t occ = friendly | opponent;

    uint64_t bb = friendly;
    while (bb) {
        uint8_t from_sq = static_cast<uint8_t>(__builtin_ctzll(bb));
        int from_f = square_file(from_sq);
        int from_r = square_rank(from_sq);

        for (int axis = 0; axis < 4; ++axis) {
            int count = __builtin_popcountll(occ & LOOKUP_TABLES.line_masks[from_sq][axis]);
            for (int d = 0; d < 2; ++d) {
                int to_f = from_f + count * AXIS_DIRECTIONS[axis][d].df;
                int to_r = from_r + count * AXIS_DIRECTIONS[axis][d].dr;

                if (to_f >= 0 && to_f < 8 && to_r >= 0 && to_r < 8) {
                    uint8_t to_sq = make_square(static_cast<uint8_t>(to_f), static_cast<uint8_t>(to_r));
                    uint64_t to_mask = 1ULL << to_sq;

                    // Cannot land on a friendly piece
                    if ((friendly & to_mask) != 0) continue;

                    // Cannot jump over opponent pieces
                    if ((LOOKUP_TABLES.between_masks[from_sq][to_sq] & opponent) != 0) continue;

                    moves.emplace_back(from_sq, to_sq);
                }
            }
        }
        bb &= bb - 1;
    }
}

std::vector<Move> Board::generate_legal_moves() const {
    std::vector<Move> moves;
    moves.reserve(48);
    generate_legal_moves(moves);
    return moves;
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

namespace {

constexpr uint64_t NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;
constexpr uint64_t NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;

int evaluate_color(uint64_t bb) {
    int count = __builtin_popcountll(bb);
    if (count <= 1) return 1000;

    int sum_f = 0;
    int sum_r = 0;
    uint64_t temp = bb;
    while (temp) {
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(temp));
        sum_f += square_file(sq);
        sum_r += square_rank(sq);
        temp &= temp - 1;
    }

    int total_dist_scaled = 0;
    temp = bb;
    int adjacencies = 0;

    while (temp) {
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(temp));
        int f = square_file(sq);
        int r = square_rank(sq);
        total_dist_scaled += std::abs(count * f - sum_f) + std::abs(count * r - sum_r);

        uint64_t piece_mask = 1ULL << sq;
        uint64_t neighbors = (piece_mask << 8)
            | (piece_mask >> 8)
            | ((piece_mask & NOT_H_FILE) << 1)
            | ((piece_mask & NOT_A_FILE) >> 1)
            | ((piece_mask & NOT_H_FILE) << 9)
            | ((piece_mask & NOT_A_FILE) << 7)
            | ((piece_mask & NOT_H_FILE) >> 7)
            | ((piece_mask & NOT_A_FILE) >> 9);
        adjacencies += __builtin_popcountll(neighbors & bb);

        temp &= temp - 1;
    }

    int avg_dist = total_dist_scaled / count;
    return -avg_dist * 10 + (adjacencies / 2) * 8 + count * 5;
}

} // anonymous namespace

bool Board::is_connected(Color c) const {
    uint64_t bb = pieces(c);
    if (!bb) return false;
    if ((bb & (bb - 1)) == 0) return true;

    uint8_t start_sq = static_cast<uint8_t>(__builtin_ctzll(bb));
    uint64_t connected = (1ULL << start_sq);
    uint64_t new_connected = connected;

    do {
        connected = new_connected;
        uint64_t dilated = connected
            | (connected << 8)
            | (connected >> 8)
            | ((connected & NOT_H_FILE) << 1)
            | ((connected & NOT_A_FILE) >> 1)
            | ((connected & NOT_H_FILE) << 9)
            | ((connected & NOT_A_FILE) << 7)
            | ((connected & NOT_H_FILE) >> 7)
            | ((connected & NOT_A_FILE) >> 9);
        new_connected = dilated & bb;
    } while (new_connected != connected);

    return connected == bb;
}

int Board::evaluate() const {
    bool me_connected = is_connected(side_to_move_);
    bool opp_connected = is_connected(~side_to_move_);

    if (me_connected && !opp_connected) return 100000;
    if (opp_connected && !me_connected) return -100000;
    if (me_connected && opp_connected) {
        // Both connected: the player who just moved won
        return -100000;
    }

    int my_score = evaluate_color(pieces(side_to_move_));
    int opp_score = evaluate_color(pieces(~side_to_move_));

    return my_score - opp_score;
}

Board Board::from_fen(const std::string& fen) {
    std::istringstream iss(fen);
    std::string placement;
    std::string side = "w";
    if (iss >> placement) {
        iss >> side;
    }
    return from_fen(placement, side);
}

Board Board::from_fen(const std::string& placement, const std::string& side_to_move) {
    uint64_t black_bb = 0;
    uint64_t white_bb = 0;

    int rank = 7;
    int file = 0;

    for (char ch : placement) {
        if (ch == '/') {
            --rank;
            file = 0;
        } else if (ch >= '1' && ch <= '8') {
            file += (ch - '0');
        } else if (file < 8 && rank >= 0) {
            uint8_t sq = make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(rank));
            uint64_t mask = 1ULL << sq;
            if (ch == 'O' || ch == 'o') {
                white_bb |= mask;
            } else if (std::isupper(static_cast<unsigned char>(ch))) {
                black_bb |= mask;
            } else if (std::islower(static_cast<unsigned char>(ch))) {
                white_bb |= mask;
            }
            ++file;
        }
    }

    Color to_move = Color::BLACK;
    if (!side_to_move.empty()) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(side_to_move[0])));
        if (c == 'b' || c == 'o') {
            to_move = Color::WHITE;
        } else {
            to_move = Color::BLACK;
        }
    }

    return Board(black_bb, white_bb, to_move);
}

void Board::load_fen(const std::string& fen) {
    *this = from_fen(fen);
}

void Board::load_fen(const std::string& placement, const std::string& side_to_move) {
    *this = from_fen(placement, side_to_move);
}

std::string Board::to_fen() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            uint8_t sq = make_square(static_cast<uint8_t>(file), static_cast<uint8_t>(rank));
            uint64_t mask = 1ULL << sq;
            if (pieces_[static_cast<size_t>(Color::BLACK)] & mask) {
                if (empty_count > 0) {
                    fen += std::to_string(empty_count);
                    empty_count = 0;
                }
                fen += 'L';
            } else if (pieces_[static_cast<size_t>(Color::WHITE)] & mask) {
                if (empty_count > 0) {
                    fen += std::to_string(empty_count);
                    empty_count = 0;
                }
                fen += 'l';
            } else {
                ++empty_count;
            }
        }
        if (empty_count > 0) {
            fen += std::to_string(empty_count);
        }
        if (rank > 0) {
            fen += '/';
        }
    }
    fen += ' ';
    fen += (side_to_move_ == Color::BLACK ? 'w' : 'b');
    fen += " - - 0 1";
    return fen;
}

bool Board::operator==(const Board& other) const {
    return pieces_[0] == other.pieces_[0] &&
           pieces_[1] == other.pieces_[1] &&
           side_to_move_ == other.side_to_move_;
}

bool Board::operator!=(const Board& other) const {
    return !(*this == other);
}

uint64_t Board::hash() const {
    uint64_t h = pieces_[0];
    h ^= pieces_[1] + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<uint64_t>(side_to_move_) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

