#pragma once

#include "Types.h"
#include <algorithm>

struct TimeControl {
    int wtime = -1;          // White time remaining in ms (-1 if not specified)
    int btime = -1;          // Black time remaining in ms (-1 if not specified)
    int winc = 0;            // White increment in ms
    int binc = 0;            // Black increment in ms
    int movestogo = 0;       // Moves to next time control (0 if sudden death / not specified)
    int movetime = -1;       // Fixed move time in ms (-1 if not specified)
    int depth = -1;          // Fixed search depth (-1 if not specified)
    int move_overhead = 100; // Latency safety buffer in ms
};

struct SearchLimits {
    bool time_limited = false;
    int soft_time_ms = 0;    // Soft target: do not start depth d+1 if elapsed >= soft_time_ms
    int hard_time_ms = 0;    // Hard cutoff: abort search if elapsed >= hard_time_ms
    int max_depth = 20;      // Max depth to search
};

class TimeManager {
public:
    static SearchLimits calculate_limits(const TimeControl& tc, Color side_to_move, int current_move);
};
