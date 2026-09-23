#pragma once

#include <cstdint>
#include <string>

enum class Color : uint8_t {
    BLACK = 0,
    WHITE = 1
};

Color operator~(Color c);
uint64_t square_to_bitboard(uint8_t sq);
uint8_t make_square(uint8_t file, uint8_t rank);
uint8_t square_file(uint8_t sq);
uint8_t square_rank(uint8_t sq);
std::string square_to_string(uint8_t sq);
uint8_t string_to_square(const std::string& s);
std::string color_to_string(Color c);
