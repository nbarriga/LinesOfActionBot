#include "Board.h"
#include "Move.h"
#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::string line;
    Board board;

    // Tell the wrapper your engine name when it initializes
    std::cout << "id name LOABot\n";
    std::cout << "id author Antigravity\n";
    std::cout << "uciok" << std::endl;

    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name LOABot\n";
            std::cout << "id author Antigravity\n";
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
            // Placeholder: currently returns first standard LOA opening move
            std::string best_move = "b1b3";
            std::cout << "bestmove " << best_move << std::endl;
        } 
        else if (line == "d" || line == "print") {
            board.print();
        }
        else if (line == "quit") {
            break;
        }
    }
    return 0;
}