#include "Board.h"
#include "Move.h"
#include "Search.h"
#include "TimeManager.h"
#include <iostream>
#include <random>
#include <sstream>
#include <string>

static std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

int main(int argc, char** argv) {
    std::string line;
    Board board;
    Search search;
    std::mt19937 rng(1337);

    SearchAlgorithm algo = SearchAlgorithm::ALPHABETA;
    int default_depth = 4;
    bool depth_explicitly_set = false;
    bool use_tt = true;
    bool order_moves = true;
    int move_overhead = 100;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--algo" && i + 1 < argc) {
            std::string a = to_lower(argv[++i]);
            if (a == "negamax") algo = SearchAlgorithm::NEGAMAX;
            else if (a == "alphabeta") algo = SearchAlgorithm::ALPHABETA;
        } else if (arg == "--depth" && i + 1 < argc) {
            default_depth = std::stoi(argv[++i]);
            depth_explicitly_set = true;
        } else if (arg == "--tt" && i + 1 < argc) {
            std::string val = to_lower(argv[++i]);
            use_tt = (val == "true" || val == "1");
        } else if (arg == "--order" && i + 1 < argc) {
            std::string val = to_lower(argv[++i]);
            order_moves = (val == "true" || val == "1");
        } else if (arg == "--overhead" && i + 1 < argc) {
            move_overhead = std::stoi(argv[++i]);
        }
    }

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line == "uci") {
            std::cout << "id name LOABot" << std::endl;
            std::cout << "id author Nicolas A. Barriga" << std::endl;
            std::cout << "option name Move Overhead type spin default 100 min 0 max 5000" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
            std::cout << "option name Algorithm type combo default alphabeta var alphabeta var negamax" << std::endl;
            std::cout << "option name Depth type spin default 4 min 1 max 20" << std::endl;
            std::cout << "option name UseTT type check default true" << std::endl;
            std::cout << "option name OrderMoves type check default true" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } 
        else if (line == "ucinewgame") {
            board = Board();
            search.clear_tt();
        }
        else if (line.rfind("setoption", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            iss >> token; // "setoption"
            std::string opt_name;
            std::string opt_val;
            bool reading_name = false;
            bool reading_val = false;

            while (iss >> token) {
                std::string lower = to_lower(token);
                if (lower == "name") {
                    reading_name = true;
                    reading_val = false;
                } else if (lower == "value") {
                    reading_name = false;
                    reading_val = true;
                } else if (reading_name) {
                    if (!opt_name.empty()) opt_name += " ";
                    opt_name += token;
                } else if (reading_val) {
                    if (!opt_val.empty()) opt_val += " ";
                    opt_val += token;
                }
            }

            std::string name_lower = to_lower(opt_name);
            std::string val_lower = to_lower(opt_val);

            if (name_lower == "algorithm") {
                if (val_lower == "negamax") {
                    algo = SearchAlgorithm::NEGAMAX;
                } else if (val_lower == "alphabeta") {
                    algo = SearchAlgorithm::ALPHABETA;
                }
            } else if (name_lower == "depth") {
                int d = std::stoi(opt_val);
                if (d >= 1) {
                    default_depth = d;
                    depth_explicitly_set = true;
                }
            } else if (name_lower == "usett") {
                use_tt = (val_lower == "true" || val_lower == "1");
            } else if (name_lower == "ordermoves") {
                order_moves = (val_lower == "true" || val_lower == "1");
            } else if (name_lower == "move overhead" || name_lower == "moveoverhead") {
                int ov = std::stoi(opt_val);
                if (ov >= 0) move_overhead = ov;
            }
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
            TimeControl tc;
            tc.move_overhead = move_overhead;

            while (iss >> token) {
                if (token == "depth") {
                    int d;
                    if (iss >> d) tc.depth = d;
                } else if (token == "wtime") {
                    int t;
                    if (iss >> t) tc.wtime = t;
                } else if (token == "btime") {
                    int t;
                    if (iss >> t) tc.btime = t;
                } else if (token == "winc") {
                    int inc;
                    if (iss >> inc) tc.winc = inc;
                } else if (token == "binc") {
                    int inc;
                    if (iss >> inc) tc.binc = inc;
                } else if (token == "movetime") {
                    int mt;
                    if (iss >> mt) tc.movetime = mt;
                } else if (token == "movestogo") {
                    int mtg;
                    if (iss >> mtg) tc.movestogo = mtg;
                }
            }

            int effective_depth = -1;
            if (tc.depth > 0) {
                effective_depth = tc.depth;
            } else if (depth_explicitly_set) {
                effective_depth = default_depth;
            }
            tc.depth = effective_depth;

            SearchLimits limits = TimeManager::calculate_limits(tc, board.turn(), board.current_move_number());
            int search_depth = (effective_depth > 0) ? effective_depth : (limits.time_limited ? 20 : default_depth);
            if (search_depth < 1) search_depth = 1;

            Color winner;
            if (board.is_game_over(winner)) {
                std::cout << "info string gameover " << color_to_string(winner) << "_wins" << std::endl;
                std::cout << "bestmove (none)" << std::endl;
            } else {
                Move chosen = search.find_best_move(board, search_depth, true, algo, order_moves, use_tt, limits);
                if (chosen == Move()) {
                    std::cout << "info string gameover " << color_to_string(~board.turn()) << "_wins" << std::endl;
                    std::cout << "bestmove (none)" << std::endl;
                } else {
                    std::cout << "bestmove " << chosen.to_uci() << std::endl;
                }
            }
        } 
        else if (line == "status" || line == "isgameover") {
            Color winner;
            if (board.is_game_over(winner)) {
                std::cout << "gameover " << color_to_string(winner) << "_wins" << std::endl;
            } else {
                std::cout << "in_progress" << std::endl;
            }
        }
        else if (line == "legalmoves" || line == "moves") {
            Color winner;
            if (board.is_game_over(winner)) {
                std::cout << "info string gameover " << color_to_string(winner) << "_wins" << std::endl;
                std::cout << "legalmoves" << std::endl;
            } else {
                auto legal = board.generate_legal_moves();
                std::cout << "legalmoves";
                for (const auto& m : legal) {
                    std::cout << " " << m.to_uci();
                }
                std::cout << std::endl;
            }
        }
        else if (line == "randommove") {
            Color winner;
            if (board.is_game_over(winner)) {
                std::cout << "info string gameover " << color_to_string(winner) << "_wins" << std::endl;
                std::cout << "bestmove (none)" << std::endl;
            } else {
                auto legal = board.generate_legal_moves();
                if (legal.empty()) {
                    std::cout << "bestmove (none)" << std::endl;
                } else {
                    std::uniform_int_distribution<size_t> dist(0, legal.size() - 1);
                    std::cout << "bestmove " << legal[dist(rng)].to_uci() << std::endl;
                }
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
            std::istringstream iss(line.substr(13));
            std::string token;
            while (iss >> token) {
                if (token == "=" || token == ":") continue;
                try {
                    int val = std::stoi(token);
                    if (val >= 0) move_overhead = val;
                    break;
                } catch (...) {}
            }
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