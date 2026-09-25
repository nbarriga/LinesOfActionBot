#include "TimeManager.h"
#include <algorithm>

SearchLimits TimeManager::calculate_limits(const TimeControl& tc, Color side_to_move, int current_move) {
    SearchLimits limits;

    // 1. Fixed move time (movetime) takes highest precedence if set
    if (tc.movetime > 0) {
        limits.time_limited = true;
        limits.soft_time_ms = std::max(10, tc.movetime - tc.move_overhead);
        limits.hard_time_ms = std::max(10, tc.movetime - tc.move_overhead / 2);
        limits.max_depth = (tc.depth > 0) ? tc.depth : 20;
        return limits;
    }

    // 2. Dynamic clock time (wtime / btime)
    int time_left = (side_to_move == Color::WHITE) ? tc.wtime : tc.btime;
    int inc = (side_to_move == Color::WHITE) ? tc.winc : tc.binc;

    if (time_left >= 0) {
        limits.time_limited = true;

        // Normal game length assumption: 20 moves per side.
        // For move < 15: divisor is (20 - current_move).
        // For move >= 15: divisor switches to fixed 5 (1/5 remaining time).
        int divisor = std::max(5, 20 - current_move);

        if (tc.movestogo > 0) {
            divisor = std::min(divisor, tc.movestogo);
        }
        if (divisor < 1) {
            divisor = 1;
        }

        int effective_time = std::max(0, time_left - tc.move_overhead);
        double base_time = static_cast<double>(effective_time) / divisor + 0.8 * inc;

        int soft = static_cast<int>(base_time);
        if (effective_time > 100) {
            soft = std::min(soft, effective_time / 2);
        }
        soft = std::max(10, soft);

        int hard = std::max(soft, static_cast<int>(soft * 1.25));
        int max_allowed = std::max(10, static_cast<int>(effective_time * 0.85));
        hard = std::min(hard, max_allowed);
        if (hard < soft) {
            soft = hard;
        }
        hard = std::max(hard, soft);

        limits.soft_time_ms = soft;
        limits.hard_time_ms = hard;
        limits.max_depth = (tc.depth > 0) ? tc.depth : 20;
        return limits;
    }

    // 3. Fixed depth only
    if (tc.depth > 0) {
        limits.time_limited = false;
        limits.max_depth = tc.depth;
        return limits;
    }

    // 4. Fallback default
    limits.time_limited = false;
    limits.max_depth = 20;
    return limits;
}
