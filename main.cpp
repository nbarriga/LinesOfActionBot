#include "Board.h"
#include "Move.h"
#include <iostream>
#include <random>
#include <sstream>
#include <string>

int main() {
    std::string line;
    Board board;
    std::mt19937 rng(1337);

    // Tell the wrapper your engine name when it initializes
    // std::cout << "id name LOABot\n";
    // std::cout << "id author Antigravity\n";
    // std::cout << "uciok" << std::endl;

    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name LOABot"<< std::endl;
            std::cout << "id author Antigravity"<< std::endl;
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
        }
        else if (line.rfind("position", 0) == 0) {
            // e.g., "position startpos moves b1b3 a2c2"
            std::istringstream iss(line);
            std::string token;
            iss >> token; // "position"
            if (iss >> token && token == "startpos") {
                board = Board();
                if (iss >> token && token == "moves") {
                    std::string move_str;
                    while (iss >> move_str) {
                        Move m = Move::from_uci(move_str);
                        board.apply_move(m);
                    }
                }
            }
        } 
        else if (line.rfind("go", 0) == 0) {
            auto moves = board.generate_legal_moves();
            if (moves.empty()) {
                std::cout << "bestmove (none)" << std::endl;
            } else {
                std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
                const Move& chosen = moves[dist(rng)];
                std::cout << "bestmove " << chosen.to_uci() << std::endl;
            }
        } 
        else if (line == "d" || line == "print") {
            board.print();
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
        }
    }
    return 0;
}