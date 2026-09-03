#include "Board.h"
#include "Move.h"
#include "Types.h"
#include <cassert>
#include <iostream>
#include <sstream>

void test_initial_board() {
    std::cout << "Running test_initial_board..." << std::endl;
    Board board;

    // Check piece counts
    assert(__builtin_popcountll(board.black_pieces()) == 12);
    assert(__builtin_popcountll(board.white_pieces()) == 12);
    assert(__builtin_popcountll(board.all_pieces()) == 24);

    // Initial turn is Black
    assert(board.turn() == Color::BLACK);

    // Corners must be empty
    assert(board.piece_at(string_to_square("a1")) == '-');
    assert(board.piece_at(string_to_square("h1")) == '-');
    assert(board.piece_at(string_to_square("a8")) == '-');
    assert(board.piece_at(string_to_square("h8")) == '-');

    // Black on ranks 1 and 8 (b1-g1, b8-g8)
    for (char f = 'b'; f <= 'g'; ++f) {
        std::string sq1 = std::string(1, f) + "1";
        std::string sq8 = std::string(1, f) + "8";
        assert(board.piece_at(string_to_square(sq1)) == 'X');
        assert(board.piece_at(string_to_square(sq8)) == 'X');
    }

    // White on files a and h (a2-a7, h2-h7)
    for (char r = '2'; r <= '7'; ++r) {
        std::string sqA = std::string("a") + r;
        std::string sqH = std::string("h") + r;
        assert(board.piece_at(string_to_square(sqA)) == 'O');
        assert(board.piece_at(string_to_square(sqH)) == 'O');
    }

    std::cout << "test_initial_board passed!\n";
}

void test_copy_constructor() {
    std::cout << "Running test_copy_constructor..." << std::endl;
    Board original;
    Board copy(original);

    assert(copy == original);
    assert(copy.black_pieces() == original.black_pieces());
    assert(copy.white_pieces() == original.white_pieces());
    assert(copy.turn() == original.turn());

    // Apply move to copy and verify original is unchanged
    Move m = Move::from_uci("b1b3");
    copy.apply_move(m);

    assert(copy != original);
    assert(original.turn() == Color::BLACK);
    assert(copy.turn() == Color::WHITE);
    assert(original.piece_at(string_to_square("b1")) == 'X');
    assert(copy.piece_at(string_to_square("b1")) == '-');
    assert(copy.piece_at(string_to_square("b3")) == 'X');

    std::cout << "test_copy_constructor passed!\n";
}

void test_move_uci() {
    std::cout << "Running test_move_uci..." << std::endl;
    Move m(string_to_square("b1"), string_to_square("b3"));
    assert(m.to_uci() == "b1b3");

    Move parsed = Move::from_uci("b1b3");
    assert(parsed == m);
    assert(parsed.from() == string_to_square("b1"));
    assert(parsed.to() == string_to_square("b3"));
    assert(parsed.from_mask() == (1ULL << string_to_square("b1")));
    assert(parsed.to_mask() == (1ULL << string_to_square("b3")));

    Move m2 = Move::from_uci("h2h5");
    assert(m2.to_uci() == "h2h5");

    std::cout << "test_move_uci passed!\n";
}

void test_apply_move_non_capture() {
    std::cout << "Running test_apply_move_non_capture..." << std::endl;
    Board board;
    Move m = Move::from_uci("b1b3");

    board.apply_move(m);

    assert(board.piece_at(string_to_square("b1")) == '-');
    assert(board.piece_at(string_to_square("b3")) == 'X');
    assert(__builtin_popcountll(board.black_pieces()) == 12);
    assert(__builtin_popcountll(board.white_pieces()) == 12);
    assert(board.turn() == Color::WHITE);

    // Now White moves a2 to c2
    Move mw = Move::from_uci("a2c2");
    board.apply_move(mw);
    assert(board.piece_at(string_to_square("a2")) == '-');
    assert(board.piece_at(string_to_square("c2")) == 'O');
    assert(board.turn() == Color::BLACK);

    std::cout << "test_apply_move_non_capture passed!\n";
}

void test_apply_move_capture() {
    std::cout << "Running test_apply_move_capture..." << std::endl;
    // Set up a custom position with a Black piece at d4 and White piece at d7
    uint64_t black = (1ULL << string_to_square("d4"));
    uint64_t white = (1ULL << string_to_square("d7"));
    Board board(black, white, Color::BLACK);

    assert(__builtin_popcountll(board.black_pieces()) == 1);
    assert(__builtin_popcountll(board.white_pieces()) == 1);

    Move capture_move = Move::from_uci("d4d7");
    board.apply_move(capture_move);

    assert(board.piece_at(string_to_square("d4")) == '-');
    assert(board.piece_at(string_to_square("d7")) == 'X');
    assert(__builtin_popcountll(board.black_pieces()) == 1);
    assert(__builtin_popcountll(board.white_pieces()) == 0);
    assert(board.turn() == Color::WHITE);

    std::cout << "test_apply_move_capture passed!\n";
}

void test_print() {
    std::cout << "Running test_print..." << std::endl;
    Board board;
    std::ostringstream oss;
    board.print(oss);
    std::string out = oss.str();

    std::string expected =
        "  +-----------------+\n"
        "8 | - X X X X X X - |\n"
        "7 | O - - - - - - O |\n"
        "6 | O - - - - - - O |\n"
        "5 | O - - - - - - O |\n"
        "4 | O - - - - - - O |\n"
        "3 | O - - - - - - O |\n"
        "2 | O - - - - - - O |\n"
        "1 | - X X X X X X - |\n"
        "  +-----------------+\n"
        "    a b c d e f g h\n"
        "Side to move: Black\n";

    assert(out == expected);
    std::cout << "test_print passed!\n";
}

int main() {
    std::cout << "=== Running Lines of Action Bot Tests ===\n";
    test_initial_board();
    test_copy_constructor();
    test_move_uci();
    test_apply_move_non_capture();
    test_apply_move_capture();
    test_print();
    std::cout << "\nAll tests passed successfully!\n";
    return 0;
}
