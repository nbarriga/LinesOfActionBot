#pragma once

#include <cstdint>
#include <string>

class Move {
public:
    Move();
    Move(uint8_t from, uint8_t to);

    uint8_t from() const;
    uint8_t to() const;

    uint64_t from_mask() const;
    uint64_t to_mask() const;

    std::string to_uci() const;
    static Move from_uci(const std::string& uci_str);

    bool operator==(const Move& other) const;
    bool operator!=(const Move& other) const;

private:
    uint8_t from_sq;
    uint8_t to_sq;
};
