#pragma once

#include "Types.h"
#include "Move.h"
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

class Board {
public:
    static constexpr uint64_t INITIAL_BLACK = 0x7E0000000000007EULL;
    static constexpr uint64_t INITIAL_WHITE = 0x0081818181818100ULL;

    Board();
    Board(uint64_t black, uint64_t white, Color to_move = Color::BLACK);
    Board(const Board& other) = default;
    Board& operator=(const Board& other) = default;

    void apply_move(const Move& move);

    std::vector<Move> generate_legal_moves() const;
    void generate_legal_moves(std::vector<Move>& moves) const;

    uint64_t pieces(Color c) const;
    uint64_t black_pieces() const;
    uint64_t white_pieces() const;
    uint64_t all_pieces() const;

    Color turn() const;
    void set_turn(Color c);

    char piece_at(uint8_t sq) const;
    void print(std::ostream& os = std::cout) const;

    bool is_connected(Color c) const;
    bool is_game_over(Color& winner) const;
    int evaluate() const;

    static Board from_fen(const std::string& fen);
    static Board from_fen(const std::string& placement, const std::string& side_to_move);
    void load_fen(const std::string& fen);
    void load_fen(const std::string& placement, const std::string& side_to_move);
    std::string to_fen() const;

    int ply_count() const { return ply_count_; }
    int current_move_number() const { return (ply_count_ / 2) + 1; }
    void set_ply_count(int plies) { ply_count_ = plies; }

    bool operator==(const Board& other) const;
    bool operator!=(const Board& other) const;

    uint64_t hash() const;

private:
    uint64_t pieces_[2];
    Color side_to_move_;
    int ply_count_ = 0;
};

namespace std {
template <>
struct hash<Board> {
    size_t operator()(const Board& b) const noexcept {
        return static_cast<size_t>(b.hash());
    }
};
}
