#include "Board.h"
#include "Move.h"
#include "Search.h"
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

void test_initial_legal_moves() {
    std::cout << "Running test_initial_legal_moves..." << std::endl;
    Board board;
    auto moves = board.generate_legal_moves();
    assert(moves.size() == 36);

    auto has_move = [&](const std::string& uci) {
        Move target = Move::from_uci(uci);
        for (const auto& m : moves) {
            if (m == target) return true;
        }
        return false;
    };

    // Verify key initial moves for Black
    assert(has_move("b1b3"));
    assert(has_move("b1h1"));
    assert(has_move("b1d3"));
    assert(has_move("c1a3")); // capture
    assert(has_move("c1c3"));
    assert(has_move("c1e3"));
    assert(has_move("d1b3"));
    assert(has_move("d1d3"));
    assert(has_move("d1f3"));

    // Verify initial moves for White
    board.set_turn(Color::WHITE);
    auto white_moves = board.generate_legal_moves();
    assert(white_moves.size() == 36);

    auto has_white_move = [&](const std::string& uci) {
        Move target = Move::from_uci(uci);
        for (const auto& m : white_moves) {
            if (m == target) return true;
        }
        return false;
    };
    assert(has_white_move("a2c2"));
    assert(has_white_move("a2a8"));
    assert(has_white_move("a2c4"));
    assert(has_white_move("a3c1")); // capture

    std::cout << "test_initial_legal_moves passed!\n";
}

void test_friendly_jump() {
    std::cout << "Running test_friendly_jump..." << std::endl;
    // Black pieces at b2, b3, b6. White has no pieces on file b.
    // Line piece count on file b is 3.
    // Piece at b2 moving north: count = 3. Target is b5.
    // Square b3 has a friendly piece, so b2 jumps over b3 to land on empty b5.
    uint64_t black = (1ULL << string_to_square("b2")) |
                     (1ULL << string_to_square("b3")) |
                     (1ULL << string_to_square("b6"));
    uint64_t white = (1ULL << string_to_square("h8")); // isolated dummy piece
    Board board(black, white, Color::BLACK);

    auto moves = board.generate_legal_moves();
    bool found_b2b5 = false;
    for (const auto& m : moves) {
        if (m.to_uci() == "b2b5") {
            found_b2b5 = true;
            break;
        }
    }
    assert(found_b2b5);
    std::cout << "test_friendly_jump passed!\n";
}

void test_enemy_block() {
    std::cout << "Running test_enemy_block..." << std::endl;
    // Black pieces at b2, b6. White piece at b3.
    // Line piece count on file b is 3.
    // Piece at b2 moving north: count = 3. Target is b5.
    // Square b3 has an opponent piece, so b2 is blocked and cannot jump over b3!
    uint64_t black = (1ULL << string_to_square("b2")) |
                     (1ULL << string_to_square("b6"));
    uint64_t white = (1ULL << string_to_square("b3"));
    Board board(black, white, Color::BLACK);

    auto moves = board.generate_legal_moves();
    for (const auto& m : moves) {
        assert(m.to_uci() != "b2b5");
    }
    std::cout << "test_enemy_block passed!\n";
}

void test_captures_and_friendly_destinations() {
    std::cout << "Running test_captures_and_friendly_destinations..." << std::endl;
    // Black at d4, White at d6.
    // File d has 2 pieces. Black at d4 moving north: count = 2. Target = d6.
    // d6 has an opponent piece, so d4d6 is a valid capture.
    uint64_t black = (1ULL << string_to_square("d4"));
    uint64_t white = (1ULL << string_to_square("d6"));
    Board board(black, white, Color::BLACK);

    auto moves = board.generate_legal_moves();
    bool captured = false;
    for (const auto& m : moves) {
        if (m.to_uci() == "d4d6") {
            captured = true;
            break;
        }
    }
    assert(captured);

    // If target d6 has a friendly piece instead, it must not be generated
    Board board_friendly((1ULL << string_to_square("d4")) | (1ULL << string_to_square("d6")),
                         (1ULL << string_to_square("a1")), Color::BLACK);
    auto moves_friendly = board_friendly.generate_legal_moves();
    for (const auto& m : moves_friendly) {
        assert(m.to_uci() != "d4d6");
    }

    std::cout << "test_captures_and_friendly_destinations passed!\n";
}

uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves;
    board.generate_legal_moves(moves);
    if (depth == 1) return moves.size();
    uint64_t nodes = 0;
    for (const auto& m : moves) {
        Board copy = board;
        copy.apply_move(m);
        nodes += perft(copy, depth - 1);
    }
    return nodes;
}

void test_perft() {
    std::cout << "Running test_perft..." << std::endl;
    Board board;
    uint64_t p1 = perft(board, 1);
    assert(p1 == 36);
    std::cout << "  perft(1) = " << p1 << " (passed)\n";

    uint64_t p2 = perft(board, 2);
    std::cout << "  perft(2) = " << p2 << "\n";
    assert(p2 > 0);
    std::cout << "test_perft passed!\n";
}

void test_fen_initial() {
    std::cout << "Running test_fen_initial..." << std::endl;
    Board initial_board;

    std::string fen = "1LLLLLL1/l6l/l6l/l6l/l6l/l6l/l6l/1LLLLLL1 w - - 0 1";
    Board from_fen_board = Board::from_fen(fen);

    assert(from_fen_board == initial_board);
    assert(from_fen_board.black_pieces() == Board::INITIAL_BLACK);
    assert(from_fen_board.white_pieces() == Board::INITIAL_WHITE);
    assert(from_fen_board.turn() == Color::BLACK);

    // Verify to_fen reproduces initial FEN
    assert(initial_board.to_fen() == fen);
    assert(from_fen_board.to_fen() == fen);

    // Also test two-argument overload
    Board from_fen_parts = Board::from_fen("1LLLLLL1/l6l/l6l/l6l/l6l/l6l/l6l/1LLLLLL1", "w");
    assert(from_fen_parts == initial_board);

    std::cout << "test_fen_initial passed!\n";
}

void test_fen_side_to_move() {
    std::cout << "Running test_fen_side_to_move..." << std::endl;
    std::string fen_black = "1LLLLLL1/l6l/l6l/l6l/l6l/l6l/l6l/1LLLLLL1 w - - 0 1";
    std::string fen_white = "1LLLLLL1/l6l/l6l/l6l/l6l/l6l/l6l/1LLLLLL1 b - - 0 1";

    Board b1 = Board::from_fen(fen_black);
    assert(b1.turn() == Color::BLACK);

    Board b2 = Board::from_fen(fen_white);
    assert(b2.turn() == Color::WHITE);
    assert(b2.black_pieces() == Board::INITIAL_BLACK);
    assert(b2.white_pieces() == Board::INITIAL_WHITE);

    std::cout << "test_fen_side_to_move passed!\n";
}

void test_fen_after_move() {
    std::cout << "Running test_fen_after_move..." << std::endl;
    Board board;
    board.apply_move(Move::from_uci("d1d3"));

    // After d1d3, side to move is White (represented as 'b' in FEN)
    assert(board.turn() == Color::WHITE);
    assert(board.piece_at(string_to_square("d1")) == '-');
    assert(board.piece_at(string_to_square("d3")) == 'X');

    std::string fen = board.to_fen();
    Board restored = Board::from_fen(fen);
    assert(restored == board);
    assert(restored.turn() == Color::WHITE);
    assert(restored.piece_at(string_to_square("d1")) == '-');
    assert(restored.piece_at(string_to_square("d3")) == 'X');

    std::cout << "test_fen_after_move passed!\n";
}

void test_is_connected() {
    std::cout << "Running test_is_connected..." << std::endl;
    Board initial_board;
    assert(!initial_board.is_connected(Color::BLACK));
    assert(!initial_board.is_connected(Color::WHITE));

    // Single piece board: connected
    uint64_t single_black = (1ULL << string_to_square("d4"));
    uint64_t single_white = (1ULL << string_to_square("a1"));
    Board single_piece_board(single_black, single_white, Color::BLACK);
    assert(single_piece_board.is_connected(Color::BLACK));
    assert(single_piece_board.is_connected(Color::WHITE));

    // Contiguous block of 3 pieces: d4, d5, e5 -> 8-connected
    uint64_t block_black = (1ULL << string_to_square("d4"))
                         | (1ULL << string_to_square("d5"))
                         | (1ULL << string_to_square("e5"));
    Board block_board(block_black, single_white, Color::BLACK);
    assert(block_board.is_connected(Color::BLACK));

    // Diagonal chain: c3, d4, e5 -> 8-connected
    uint64_t diag_black = (1ULL << string_to_square("c3"))
                        | (1ULL << string_to_square("d4"))
                        | (1ULL << string_to_square("e5"));
    Board diag_board(diag_black, single_white, Color::BLACK);
    assert(diag_board.is_connected(Color::BLACK));

    // Separated pieces: a1 and h8 -> not connected
    uint64_t split_black = (1ULL << string_to_square("a1"))
                         | (1ULL << string_to_square("h8"));
    Board split_board(split_black, single_white, Color::BLACK);
    assert(!split_board.is_connected(Color::BLACK));

    std::cout << "test_is_connected passed!\n";
}

void test_board_evaluate() {
    std::cout << "Running test_board_evaluate..." << std::endl;
    Board initial_board;
    // Initial board is completely symmetric between players
    assert(initial_board.evaluate() == 0);

    // Initial board with white to move is also symmetric (score 0)
    initial_board.set_turn(Color::WHITE);
    assert(initial_board.evaluate() == 0);

    // Winning connected position for Black
    uint64_t conn_black = (1ULL << string_to_square("d4"))
                        | (1ULL << string_to_square("d5"));
    uint64_t split_white = (1ULL << string_to_square("a1"))
                         | (1ULL << string_to_square("h8"));
    Board win_board(conn_black, split_white, Color::BLACK);
    assert(win_board.evaluate() == 100000);

    // From White's perspective, Black is connected so White loses
    win_board.set_turn(Color::WHITE);
    assert(win_board.evaluate() == -100000);

    std::cout << "test_board_evaluate passed!\n";
}

void test_search_negamax() {
    std::cout << "Running test_search_negamax..." << std::endl;
    Board board;
    Search search;

    // Depth 1: Root (1) + 36 legal moves = 37 nodes visited
    Move m1 = search.find_best_move(board, 1, false);
    assert(m1 != Move());
    assert(search.nodes_visited() == 37);

    // Depth 2: Root (1) + 36 depth-1 + 1244 depth-2 = 1281 nodes visited
    search.reset();
    Move m2 = search.find_best_move(board, 2, false);
    assert(m2 != Move());
    assert(search.nodes_visited() == 1 + 36 + 1244);

    // Test finding an immediate winning move:
    // b4 and d4 for Black, g4 and a1 for White
    // Line on rank 4 has 3 pieces (b4, d4, g4).
    // b4 moves 3 squares east to e4 (jumping over friendly d4).
    // e4 and d4 form a connected group -> instant win!
    uint64_t winning_black = (1ULL << string_to_square("b4"))
                           | (1ULL << string_to_square("d4"));
    uint64_t winning_white = (1ULL << string_to_square("g4"))
                           | (1ULL << string_to_square("a1"));
    Board win_puzzle(winning_black, winning_white, Color::BLACK);
    Search win_search;
    Move win_move = win_search.find_best_move(win_puzzle, 1, false);
    assert(win_move.to_uci() == "b4e4");
    assert(win_search.best_score() >= 99990);

    std::cout << "test_search_negamax passed!\n";
}

void test_iterative_deepening() {
    std::cout << "Running test_iterative_deepening..." << std::endl;
    Board board;
    Search search;

    // Standard depth 2 search without iterative deepening: 1281 nodes
    Move m_std = search.find_best_move(board, 2, false);
    uint64_t nodes_std = search.nodes_visited();
    assert(nodes_std == 1281);

    // Iterative deepening search to depth 2:
    // Iteration 1: 37 nodes (1 root + 36 depth-0 leaves)
    // Iteration 2: 1281 nodes (1 root + 36 depth-1 + 1244 depth-0 leaves)
    // Total nodes: 37 + 1281 = 1318 nodes
    search.reset();
    Move m_id = search.find_best_move(board, 2, true);
    uint64_t nodes_id = search.nodes_visited();
    assert(nodes_id == 37 + 1281);
    assert(m_id == m_std);

    std::cout << "test_iterative_deepening passed!\n";
}

int main() {
    std::cout << "=== Running Lines of Action Bot Tests ===\n";
    test_initial_board();
    test_copy_constructor();
    test_move_uci();
    test_apply_move_non_capture();
    test_apply_move_capture();
    test_print();
    test_initial_legal_moves();
    test_friendly_jump();
    test_enemy_block();
    test_captures_and_friendly_destinations();
    test_perft();
    test_fen_initial();
    test_fen_side_to_move();
    test_fen_after_move();
    test_is_connected();
    test_board_evaluate();
    test_search_negamax();
    test_iterative_deepening();
    std::cout << "\nAll tests passed successfully!\n";
    return 0;
}

