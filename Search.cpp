#include "Search.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

Search::Search() : nodes_visited_(0), best_score_(0) {}

void Search::reset() {
    nodes_visited_ = 0;
    best_score_ = 0;
}

uint64_t Search::nodes_visited() const {
    return nodes_visited_;
}

int Search::best_score() const {
    return best_score_;
}

int Search::negamax(Board& board, int depth, int ply) {
    ++nodes_visited_;

    // Check terminal conditions
    bool me_connected = board.is_connected(board.turn());
    bool opp_connected = board.is_connected(~board.turn());

    if (me_connected && !opp_connected) {
        return 100000 - ply;
    }
    if (opp_connected) {
        // If opponent is connected, or both connected: opponent won
        return -100000 + ply;
    }

    if (depth <= 0) {
        return board.evaluate();
    }

    std::vector<Move> moves = board.generate_legal_moves();
    if (moves.empty()) {
        return board.evaluate();
    }

    int max_score = -INF;
    for (const auto& move : moves) {
        Board next_board = board;
        next_board.apply_move(move);

        int score = -negamax(next_board, depth - 1, ply + 1);
        if (score > max_score) {
            max_score = score;
        }
    }

    return max_score;
}

namespace {

void sort_moves_by_eval(const Board& board, std::vector<Move>& moves) {
    std::vector<std::pair<int, Move>> scored_moves;
    scored_moves.reserve(moves.size());

    for (const auto& move : moves) {
        Board next_board = board;
        next_board.apply_move(move);
        int eval = -next_board.evaluate();
        scored_moves.emplace_back(eval, move);
    }

    std::sort(scored_moves.begin(), scored_moves.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    for (size_t i = 0; i < moves.size(); ++i) {
        moves[i] = scored_moves[i].second;
    }
}

} // anonymous namespace

int Search::alphabeta(Board& board, int depth, int alpha, int beta, int ply, bool order_moves) {
    ++nodes_visited_;

    // Check terminal conditions
    bool me_connected = board.is_connected(board.turn());
    bool opp_connected = board.is_connected(~board.turn());

    if (me_connected && !opp_connected) {
        return 100000 - ply;
    }
    if (opp_connected) {
        return -100000 + ply;
    }

    if (depth <= 0) {
        return board.evaluate();
    }

    std::vector<Move> moves = board.generate_legal_moves();
    if (moves.empty()) {
        return board.evaluate();
    }

    if (order_moves) {
        sort_moves_by_eval(board, moves);
    }

    int max_score = -INF;
    for (const auto& move : moves) {
        Board next_board = board;
        next_board.apply_move(move);

        int score = -alphabeta(next_board, depth - 1, -beta, -alpha, ply + 1, order_moves);
        if (score > max_score) {
            max_score = score;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (score >= beta) {
            return score; // Fail-soft beta cutoff
        }
    }

    return max_score;
}

Move Search::find_best_move(Board& board, int depth, bool iterative_deepening,
                           SearchAlgorithm algo, bool order_moves) {
    reset();

    std::vector<Move> moves = board.generate_legal_moves();
    if (moves.empty()) {
        return Move();
    }

    Move best_move = moves[0];
    int start_depth = iterative_deepening ? 1 : depth;

    if (iterative_deepening) {
        std::cerr << "Depth \ttime\tNodes\tnodes/sec \t\tAvg.-BF  Principal var.   eval\n";
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t prev_nodes = 0;

    for (int d = start_depth; d <= depth; ++d) {
        ++nodes_visited_;

        if (order_moves && algo == SearchAlgorithm::ALPHABETA) {
            sort_moves_by_eval(board, moves);
        }

        int max_score = -INF;
        Move current_best = moves[0];
        int alpha = -INF;
        int beta = INF;

        for (const auto& move : moves) {
            Board next_board = board;
            next_board.apply_move(move);

            int score;
            if (algo == SearchAlgorithm::NEGAMAX) {
                score = -negamax(next_board, d - 1, 1);
            } else {
                score = -alphabeta(next_board, d - 1, -beta, -alpha, 1, order_moves);
            }

            if (score > max_score) {
                max_score = score;
                current_best = move;
            }
            if (algo == SearchAlgorithm::ALPHABETA) {
                if (score > alpha) {
                    alpha = score;
                }
                if (score >= beta) {
                    break;
                }
            }
        }

        best_score_ = max_score;
        best_move = current_best;

        // Place best move first if move ordering is not overriding it
        if (!order_moves || algo != SearchAlgorithm::ALPHABETA) {
            auto it = std::find(moves.begin(), moves.end(), best_move);
            if (it != moves.end() && it != moves.begin()) {
                std::iter_swap(moves.begin(), it);
            }
        }

        if (iterative_deepening) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
            uint64_t current_nodes = nodes_visited_;
            double nps = (elapsed_sec > 0.0) ? (current_nodes / elapsed_sec) : 0.0;

            std::string bf_str;
            if (d == 1 || prev_nodes == 0) {
                bf_str = "-nan";
            } else {
                double avg_bf = static_cast<double>(current_nodes) / static_cast<double>(prev_nodes);
                std::ostringstream bf_ss;
                bf_ss << std::fixed << std::setprecision(3) << avg_bf;
                bf_str = bf_ss.str();
            }
            prev_nodes = current_nodes;

            std::ostringstream time_ss;
            time_ss << std::fixed << std::setprecision(3) << elapsed_sec;

            std::ostringstream nps_ss;
            nps_ss << std::scientific << std::setprecision(2) << nps;

            std::string pv_str = "[" + best_move.to_uci() + " unkn unkn]";

            std::cerr << d << "   \t"
                      << time_ss.str() << "  \t"
                      << current_nodes << " \t"
                      << nps_ss.str() << "        \t"
                      << bf_str << "  "
                      << pv_str << "   "
                      << best_score_ << "\n";
        }
    }

    return best_move;
}
