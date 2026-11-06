#pragma once
#include <string>
#include "process.h"
#include "scheduler.h"
#include "stats.h"

// Which run produced the result; echoed back in the response.
struct RunInfo {
    std::string algorithm;   // "rr" or "priority"
    int quantum = 0;         // used when algorithm == "rr"
    bool preemptive = false; // used when algorithm == "priority"
};

// Builds the response JSON by hand:
//   algorithm, quantum | preemptive,
//   gantt:     [{pid, start, end, idle, queue}]   (idle entries have pid null)
//   processes: [{pid, arrival, burst, priority, completion, turnaround, waiting}]
//   averageWaiting, averageTurnaround, totalTime, idleTime
// Processes appear in the order they were entered.
std::string serializeResult(const ProcessList& list, const ScheduleResult& result,
                            const Stats& stats, const RunInfo& info);
