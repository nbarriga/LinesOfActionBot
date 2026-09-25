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
    stop_search_ = false;
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

    if (time_limited_ && (nodes_visited_ & 2047) == 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        if (elapsed >= hard_deadline_ms_) {
            stop_search_ = true;
            return 0;
        }
    }

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
        if (stop_search_) {
            return 0;
        }
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

    if (time_limited_ && (nodes_visited_ & 2047) == 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        if (elapsed >= hard_deadline_ms_) {
            stop_search_ = true;
            return 0;
        }
    }

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
            if (stop_search_) {
                return 0;
            }
            if (score > max_score) {
                max_score = score;
                best_move = moves[0];
            }
            if (score > alpha) {
                alpha = score;
            }
            if (score >= beta) {
                // Beta cutoff! Don't waste time sorting or searching remaining moves
                if (use_tt && !stop_search_) {
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
        if (stop_search_) {
            return 0;
        }
        if (score > max_score) {
            max_score = score;
            best_move = moves[i];
        }
        if (score > alpha) {
            alpha = score;
        }
        if (score >= beta) {
            if (use_tt && !stop_search_) {
                tt_[board] = TTEntry{depth, score, TTFlag::UPPER_BOUND, best_move};
            }
            return score;
        }
    }

    // 5. Store entry in TT
    if (use_tt && !stop_search_) {
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
                           SearchAlgorithm algo, bool order_moves, bool use_tt,
                           const SearchLimits& limits) {
    reset();

    time_limited_ = limits.time_limited;
    soft_deadline_ms_ = std::chrono::milliseconds(limits.soft_time_ms);
    hard_deadline_ms_ = std::chrono::milliseconds(limits.hard_time_ms);
    start_time_ = std::chrono::high_resolution_clock::now();

    int target_depth = depth;
    if (limits.max_depth > 0) {
        target_depth = std::min(target_depth, limits.max_depth);
    }

    Color winner;
    if (board.is_game_over(winner)) {
        return Move();
    }

    std::vector<Move> moves = board.generate_legal_moves();
    if (moves.empty()) {
        return Move();
    }

    Move best_move = moves[0];
    int start_depth = iterative_deepening ? 1 : target_depth;

    if (iterative_deepening) {
        std::cerr << "Depth \ttime\tNodes\tnodes/sec \t\tAvg.-BF  Principal var.   eval\n";
    }

    uint64_t prev_nodes = 0;
    double prev_elapsed_sec = 0.0;

    for (int d = start_depth; d <= target_depth; ++d) {
        ++nodes_visited_;

        int max_score = -INF;
        Move current_best = moves[0];
        int alpha = -INF;
        int beta = INF;
        size_t start_idx = 0;

        bool new_best_completed_at_this_depth = false;
        Move completed_better_move = Move();
        int completed_better_score = -INF;

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
                    if (stop_search_) {
                        break;
                    }
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
                        if (use_tt && !stop_search_) {
                            tt_[board] = TTEntry{d, score, TTFlag::UPPER_BOUND, best_move};
                        }
                        goto iteration_stats;
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

            if (stop_search_) {
                // Interrupted while searching moves[i]!
                // moves[i] was not completely evaluated, so discard its score.
                break;
            }

            // moves[i] finished evaluation completely!
            if (score > max_score) {
                max_score = score;
                current_best = moves[i];
                if (i > 0) {
                    new_best_completed_at_this_depth = true;
                    completed_better_move = moves[i];
                    completed_better_score = score;
                }
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

        if (stop_search_) {
            // Timeout occurred at depth d!
            if (new_best_completed_at_this_depth) {
                // A new move was fully evaluated at depth d and beat the previous best.
                best_move = completed_better_move;
                best_score_ = completed_better_score;
            }
            // Otherwise, best_move remains the one from the previous completed depth.
            break;
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

    iteration_stats:
        if (iterative_deepening) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - start_time_).count();
            elapsed_time_ = elapsed_sec;
            depth_stats_.push_back({d, nodes_visited_, elapsed_sec, best_score_, best_move});
            uint64_t current_nodes = nodes_visited_;
            double nps = (elapsed_sec > 0.0) ? (current_nodes / elapsed_sec) : 0.0;

            std::string bf_str;
            double measured_bf = 6.0;
            if (d == 1 || prev_nodes == 0) {
                bf_str = "-nan";
            } else {
                measured_bf = static_cast<double>(current_nodes) / static_cast<double>(prev_nodes);
                std::ostringstream bf_ss;
                bf_ss << std::fixed << std::setprecision(3) << measured_bf;
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

            // Check soft time limit & predict if next depth can complete
            if (time_limited_) {
                double iter_time_sec = elapsed_sec - prev_elapsed_sec;
                prev_elapsed_sec = elapsed_sec;

                double soft_sec = std::chrono::duration<double>(soft_deadline_ms_).count();

                // In Lines of Action, effective branching factor is typically 5x to 10x.
                double bf = std::max(5.0, measured_bf);
                double estimated_next_sec = elapsed_sec + iter_time_sec * bf;

                if (elapsed_sec >= soft_sec * 0.35 || estimated_next_sec > soft_sec) {
                    break;
                }
            } else {
                prev_elapsed_sec = elapsed_sec;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    elapsed_time_ = std::chrono::duration<double>(end_time - start_time_).count();

    return best_move;
}
