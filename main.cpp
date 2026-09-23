#include "Board.h"
#include "Move.h"
#include "Search.h"
#include <iostream>
#include <random>
#include <sstream>
#include <string>

int main() {
    std::string line;
    Board board;
    Search search;
    std::mt19937 rng(1337);

    // Tell the wrapper your engine name when it initializes
    // std::cout << "id name LOABot\n";
    // std::cout << "id author Antigravity\n";
    // std::cout << "uciok" << std::endl;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line == "uci") {
            std::cout << "id name LOABot"<< std::endl;
            std::cout << "id author Nicolas A. Barriga"<< std::endl;
            std::cout << "option name Move Overhead type spin default 100 min 0 max 5000" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } 
        else if (line == "ucinewgame") {
            board = Board();
            search.clear_tt();
        }
        else if (line.rfind("position", 0) == 0) {
            // e.g., "position startpos moves b1b3 a2c2"
            // or "position fen 1LLLLLL1/l6l/l6l/l6l/l6l/l6l/l6l/1LLLLLL1 w - - 0 1 moves d1d3"
            std::istringstream iss(line);
            std::string token;
            iss >> token; // "position"
            if (iss >> token) {
                if (token == "startpos") {
                    board = Board();
                    iss >> token;
                } else if (token == "fen") {
                    std::string fen_placement;
                    if (iss >> fen_placement) {
                        std::string fen_color = "w";
                        bool has_color = false;
                        while (iss >> token) {
                            if (token == "moves") {
                                break;
                            }
                            if (!has_color) {
                                fen_color = token;
                                has_color = true;
                            }
                        }
                        board = Board::from_fen(fen_placement, fen_color);
                    }
                }

                if (token == "moves") {
                    std::string move_str;
                    while (iss >> move_str) {
                        Move m = Move::from_uci(move_str);
                        board.apply_move(m);
                    }
                }
            }
        } 
        else if (line.rfind("go", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            iss >> token; // "go"
            int depth = 2; // Default depth for pure negamax
            while (iss >> token) {
                if (token == "depth") {
                    int d;
                    if (iss >> d) {
                        depth = d;
                    }
                }
            }
            if (depth < 1) depth = 1;

            auto moves = board.generate_legal_moves();
            if (moves.empty()) {
                std::cout << "bestmove (none)" << std::endl;
            } else {
                Move chosen = search.find_best_move(board, depth);
                std::cout << "bestmove " << chosen.to_uci() << std::endl;
            }
        } 
        else if (line == "d" || line == "print") {
            board.print();
        }
        else if (line == "fen") {
            std::cout << board.to_fen() << std::endl;
        }
        else if (line == "quit") {
            break;
        }
        else if (line.rfind("Move Overhead", 0) == 0) {
            std::cout << "debug: Move Overhead " << line << std::endl;
        }
        else if (line.rfind("Threads", 0) == 0) {
            std::cout << "debug: Threads " << line << std::endl;
        }
        else if (line.rfind("Hash", 0) == 0) {
            std::cout << "debug: Hash " << line << std::endl;
        }else{
            std::cout << "debug: Unknown command " << line << std::endl;
        }
    }
    return 0;
}