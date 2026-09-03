#include "Move.h"
#include "Types.h"
#include <cassert>

Move::Move() : from_sq(0), to_sq(0) {}

Move::Move(uint8_t from, uint8_t to) : from_sq(from), to_sq(to) {
    assert(from < 64 && to < 64);
}

uint8_t Move::from() const {
    return from_sq;
}

uint8_t Move::to() const {
    return to_sq;
}

uint64_t Move::from_mask() const {
    return 1ULL << from_sq;
}

uint64_t Move::to_mask() const {
    return 1ULL << to_sq;
}

std::string Move::to_uci() const {
    return square_to_string(from_sq) + square_to_string(to_sq);
}

Move Move::from_uci(const std::string& uci_str) {
    assert(uci_str.length() >= 4);
    uint8_t from = string_to_square(uci_str.substr(0, 2));
    uint8_t to = string_to_square(uci_str.substr(2, 2));
    return Move(from, to);
}

bool Move::operator==(const Move& other) const {
    return from_sq == other.from_sq && to_sq == other.to_sq;
}

bool Move::operator!=(const Move& other) const {
    return !(*this == other);
}
