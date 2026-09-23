#include "Search.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

Search::Search() : nodes_visited_(0), best_score_(0), elapsed_time_(0.0) {}

void Search::reset() {
    nodes_visited_ = 0;
    best_score_ = 0;
    elapsed_time_ = 0.0;
    depth_stats_.clear();
}

uint64_t Search::nodes_visited() const {
    return nodes_visited_;
}

int Search::best_score() const {
    return best_score_;
}

double Search::elapsed_time() const {
    return elapsed_time_;
}

const std::vector<DepthStats>& Search::depth_stats() const {
    return depth_stats_;
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

void Search::clear_tt() {
    tt_.clear();
}

size_t Search::tt_size() const {
    return tt_.size();
}

std::string Search::extract_pv(const Board& root_board, int max_plies) const {
    Board b = root_board;
    std::string pv_str = "[";
    int count = 0;

    while (count < max_plies) {
        auto it = tt_.find(b);
        if (it == tt_.end() || it->second.best_move == Move()) {
            break;
        }
        Move m = it->second.best_move;
        if (count > 0) pv_str += " ";
        pv_str += m.to_uci();
        b.apply_move(m);
        ++count;
    }

    while (count < max_plies) {
        if (count > 0) pv_str += " ";
        pv_str += "unkn";
        ++count;
    }
    pv_str += "]";
    return pv_str;
}

namespace {

void sort_remaining_moves(const Board& board, std::vector<Move>& moves, size_t start_idx,
                          const std::unordered_map<Board, TTEntry>& tt, bool use_tt) {
    if (start_idx >= moves.size()) return;

    std::vector<std::pair<int, Move>> scored_remaining;
    scored_remaining.reserve(moves.size() - start_idx);

    for (size_t i = start_idx; i < moves.size(); ++i) {
        Board next_board = board;
        next_board.apply_move(moves[i]);
        int move_eval;

        if (use_tt) {
            auto it = tt.find(next_board);
            if (it != tt.end()) {
                move_eval = -it->second.score;
            } else {
                move_eval = -next_board.evaluate();
            }
        } else {
            move_eval = -next_board.evaluate();
        }
        scored_remaining.emplace_back(move_eval, moves[i]);
    }

    std::sort(scored_remaining.begin(), scored_remaining.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    for (size_t i = 0; i < scored_remaining.size(); ++i) {
        moves[start_idx + i] = scored_remaining[i].second;
    }
}

} // anonymous namespace

int Search::alphabeta(Board& board, int depth, int alpha, int beta, int ply, bool order_moves, bool use_tt) {
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

    // 1. Transposition Table lookup
    int orig_alpha = alpha;
    Move tt_move;
    if (use_tt) {
        auto tt_it = tt_.find(board);
        if (tt_it != tt_.end()) {
            const TTEntry& entry = tt_it->second;
            tt_move = entry.best_move;
            if (entry.depth >= depth) {
                if (entry.flag == TTFlag::EXACT) {
                    return entry.score;
                } else if (entry.flag == TTFlag::LOWER_BOUND && entry.score <= alpha) {
                    return entry.score;
                } else if (entry.flag == TTFlag::UPPER_BOUND && entry.score >= beta) {
                    return entry.score;
                }
            }
        }
    }

    std::vector<Move> moves = board.generate_legal_moves();
    if (moves.empty()) {
        return board.evaluate();
    }

    int max_score = -INF;
    Move best_move = moves[0];
    size_t start_idx = 0;

    // 2. Hash move heuristic: search TT move FIRST if available
    if (use_tt && tt_move != Move()) {
        auto it = std::find(moves.begin(), moves.end(), tt_move);
        if (it != moves.end()) {
            std::iter_swap(moves.begin(), it);

            Board next_board = board;
            next_board.apply_move(moves[0]);

            int score = -alphabeta(next_board, depth - 1, -beta, -alpha, ply + 1, order_moves, use_tt);
            if (score > max_score) {
                max_score = score;
                best_move = moves[0];
            }
            if (score > alpha) {
                alpha = score;
            }
            if (score >= beta) {
                // Beta cutoff! Don't waste time sorting or searching remaining moves
                if (use_tt) {
                    tt_[board] = TTEntry{depth, score, TTFlag::UPPER_BOUND, best_move};
                }
                return score;
            }

            start_idx = 1;
        }
    }

    // 3. If TT move did not cutoff, sort remaining moves if requested
    if (order_moves) {
        sort_remaining_moves(board, moves, start_idx, tt_, use_tt);
    }

    // 4. Search remaining moves
    for (size_t i = start_idx; i < moves.size(); ++i) {
        Board next_board = board;
        next_board.apply_move(moves[i]);

        int score = -alphabeta(next_board, depth - 1, -beta, -alpha, ply + 1, order_moves, use_tt);
        if (score > max_score) {
            max_score = score;
            best_move = moves[i];
        }
        if (score > alpha) {
            alpha = score;
        }
        if (score >= beta) {
            if (use_tt) {
                tt_[board] = TTEntry{depth, score, TTFlag::UPPER_BOUND, best_move};
            }
            return score;
        }
    }

    // 5. Store entry in TT
    if (use_tt) {
        TTFlag flag;
        if (max_score <= orig_alpha) {
            flag = TTFlag::LOWER_BOUND;
        } else {
            flag = TTFlag::EXACT;
        }
        tt_[board] = TTEntry{depth, max_score, flag, best_move};
    }

    return max_score;
}

Move Search::find_best_move(Board& board, int depth, bool iterative_deepening,
                           SearchAlgorithm algo, bool order_moves, bool use_tt) {
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

        int max_score = -INF;
        Move current_best = moves[0];
        int alpha = -INF;
        int beta = INF;
        size_t start_idx = 0;

        if (algo == SearchAlgorithm::ALPHABETA) {
            Move tt_move;
            if (use_tt) {
                auto tt_it = tt_.find(board);
                if (tt_it != tt_.end()) {
                    tt_move = tt_it->second.best_move;
                }
            }

            if (use_tt && tt_move != Move()) {
                auto it = std::find(moves.begin(), moves.end(), tt_move);
                if (it != moves.end()) {
                    std::iter_swap(moves.begin(), it);

                    Board next_board = board;
                    next_board.apply_move(moves[0]);

                    int score = -alphabeta(next_board, d - 1, -beta, -alpha, 1, order_moves, use_tt);
                    if (score > max_score) {
                        max_score = score;
                        current_best = moves[0];
                    }
                    if (score > alpha) {
                        alpha = score;
                    }
                    if (score >= beta) {
                        best_score_ = score;
                        best_move = current_best;
                        if (use_tt) {
                            tt_[board] = TTEntry{d, score, TTFlag::UPPER_BOUND, best_move};
                        }
                        continue;
                    }
                    start_idx = 1;
                }
            }

            if (order_moves) {
                sort_remaining_moves(board, moves, start_idx, tt_, use_tt);
            }
        }

        for (size_t i = start_idx; i < moves.size(); ++i) {
            Board next_board = board;
            next_board.apply_move(moves[i]);

            int score;
            if (algo == SearchAlgorithm::NEGAMAX) {
                score = -negamax(next_board, d - 1, 1);
            } else {
                score = -alphabeta(next_board, d - 1, -beta, -alpha, 1, order_moves, use_tt);
            }

            if (score > max_score) {
                max_score = score;
                current_best = moves[i];
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

        if (algo == SearchAlgorithm::ALPHABETA && use_tt) {
            tt_[board] = TTEntry{d, best_score_, TTFlag::EXACT, best_move};
        } else {
            auto it = std::find(moves.begin(), moves.end(), best_move);
            if (it != moves.end() && it != moves.begin()) {
                std::iter_swap(moves.begin(), it);
            }
        }

        if (iterative_deepening) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
            elapsed_time_ = elapsed_sec;
            depth_stats_.push_back({d, nodes_visited_, elapsed_sec, best_score_, best_move});
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

            std::string pv_str;
            if (algo == SearchAlgorithm::ALPHABETA && use_tt) {
                pv_str = extract_pv(board, 3);
            } else {
                pv_str = "[" + best_move.to_uci() + " unkn unkn]";
            }

            std::cerr << d << "   \t"
                      << time_ss.str() << "  \t"
                      << current_nodes << " \t"
                      << nps_ss.str() << "        \t"
                      << bf_str << "  "
                      << pv_str << "   "
                      << best_score_ << "\n";
        }
    }

    if (!iterative_deepening) {
        auto end_time = std::chrono::high_resolution_clock::now();
        elapsed_time_ = std::chrono::duration<double>(end_time - start_time).count();
    }

    return best_move;
}
