#pragma once

#include "Types.h"
#include "Move.h"
#include <cstdint>
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

    bool operator==(const Board& other) const;
    bool operator!=(const Board& other) const;

private:
    uint64_t pieces_[2];
    Color side_to_move_;
};
