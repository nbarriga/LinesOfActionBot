#include "Types.h"
#include <cassert>

Color operator~(Color c) {
    return (c == Color::BLACK) ? Color::WHITE : Color::BLACK;
}

uint64_t square_to_bitboard(uint8_t sq) {
    assert(sq < 64);
    return 1ULL << sq;
}

uint8_t make_square(uint8_t file, uint8_t rank) {
    assert(file < 8 && rank < 8);
    return rank * 8 + file;
}

uint8_t square_file(uint8_t sq) {
    assert(sq < 64);
    return sq % 8;
}

uint8_t square_rank(uint8_t sq) {
    assert(sq < 64);
    return sq / 8;
}

std::string square_to_string(uint8_t sq) {
    assert(sq < 64);
    char f = static_cast<char>('a' + square_file(sq));
    char r = static_cast<char>('1' + square_rank(sq));
    return std::string{f, r};
}

uint8_t string_to_square(const std::string& s) {
    assert(s.length() >= 2);
    uint8_t file = static_cast<uint8_t>(s[0] - 'a');
    uint8_t rank = static_cast<uint8_t>(s[1] - '1');
    assert(file < 8 && rank < 8);
    return make_square(file, rank);
}

std::string color_to_string(Color c) {
    return (c == Color::BLACK) ? "black" : "white";
}
