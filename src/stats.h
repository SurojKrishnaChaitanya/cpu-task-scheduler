#pragma once
#include "process.h"

struct Stats {
    int    count    = 0;
    double totalTAT = 0;
    double totalWT  = 0;
    double totalRT  = 0;   // response time = start - arrival
    double avgRT    = 0;
    double avgTAT   = 0;
    double avgWT    = 0;
};

// The two formulas, kept in one place.
inline int turnaroundTime(int completion, int arrival) { return completion - arrival; }
inline int waitingTime(int turnaround, int burst)      { return turnaround - burst; }

// Walks the list. For every node (whose completion time must already be set
// by a scheduler) fills turnaround and waiting, then returns the averages.
Stats computeStats(ProcessList& list);
