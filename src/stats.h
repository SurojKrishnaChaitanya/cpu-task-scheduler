#pragma once
#include "process.h"

struct Stats {
    int    count    = 0;
    double totalTAT = 0;
    double totalWT  = 0;
    double totalRT  = 0;
    double avgRT    = 0;
    double avgTAT   = 0;
    double avgWT    = 0;
};

inline int turnaroundTime(int completion, int arrival) { return completion - arrival; }
inline int waitingTime(int turnaround, int burst)      { return turnaround - burst; }

Stats computeStats(ProcessList& list);