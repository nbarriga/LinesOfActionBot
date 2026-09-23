#pragma once

#include "Board.h"
#include "Move.h"
#include <cstdint>

enum class SearchAlgorithm {
    NEGAMAX,
    ALPHABETA
};

class Search {
public:
    static constexpr int INF = 1000000;

    Search();

    int negamax(Board& board, int depth, int ply = 0);
    int alphabeta(Board& board, int depth, int alpha, int beta, int ply = 0, bool order_moves = false);
    Move find_best_move(Board& board, int depth, bool iterative_deepening = true,
                       SearchAlgorithm algo = SearchAlgorithm::ALPHABETA, bool order_moves = false);

    uint64_t nodes_visited() const;
    int best_score() const;
    void reset();

private:
    uint64_t nodes_visited_;
    int best_score_;
};
