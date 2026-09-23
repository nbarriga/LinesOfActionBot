#pragma once

#include "Board.h"
#include "Move.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class SearchAlgorithm {
    NEGAMAX,
    ALPHABETA
};

enum class TTFlag : uint8_t {
    EXACT,
    LOWER_BOUND,
    UPPER_BOUND
};

struct TTEntry {
    int depth;
    int score;
    TTFlag flag;
    Move best_move;
};

struct DepthStats {
    int depth;
    uint64_t nodes;
    double time_sec;
    int score;
    Move best_move;
};

class Search {
public:
    static constexpr int INF = 1000000;

    Search();

    int negamax(Board& board, int depth, int ply = 0);
    int alphabeta(Board& board, int depth, int alpha, int beta, int ply = 0, bool order_moves = false, bool use_tt = true);
    Move find_best_move(Board& board, int depth, bool iterative_deepening = true,
                       SearchAlgorithm algo = SearchAlgorithm::ALPHABETA, bool order_moves = false,
                       bool use_tt = true);

    uint64_t nodes_visited() const;
    int best_score() const;
    double elapsed_time() const;
    const std::vector<DepthStats>& depth_stats() const;
    void reset();

    void clear_tt();
    size_t tt_size() const;
    std::string extract_pv(const Board& root_board, int max_plies = 3) const;

private:
    uint64_t nodes_visited_;
    int best_score_;
    double elapsed_time_;
    std::unordered_map<Board, TTEntry> tt_;
    std::vector<DepthStats> depth_stats_;
};
