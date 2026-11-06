#pragma once
#include <string>
#include <utility>
#include <vector>
#include "process.h"

// One block of the Gantt chart. pid == IDLE_PID means the CPU was idle.
constexpr int IDLE_PID = -1;

struct GanttEntry {
    int pid;
    int start;
    int end;
    // PIDs waiting when this slice began, next-to-run first (empty for idle).
    std::vector<int> queue;

    GanttEntry(int p, int s, int e, std::vector<int> q = {})
        : pid(p), start(s), end(e), queue(std::move(q)) {}
};

struct ScheduleResult {
    std::vector<GanttEntry> gantt;
    int totalTime = 0;   // clock value when the last process finished
    int idleTime  = 0;   // total length of idle gaps
    std::string error;   // empty on success
    bool ok() const { return error.empty(); }
};

// Every scheduler resets the list's results, simulates, and fills in each
// process's remaining/start/completion. Turnaround and waiting are NOT
// computed here; call computeStats() afterwards.

// Round Robin with the given time quantum (>= 1).
ScheduleResult roundRobin(ProcessList& list, int quantum);

// Priority scheduling, non-preemptive: once a process gets the CPU it runs to
// completion. Higher priority number = more urgent.
ScheduleResult priorityNonPreemptive(ProcessList& list);

// Priority scheduling, preemptive: whenever a process arrives with a strictly
// higher priority than the running one, the running process goes back into the
// heap (keeping its remaining time) and the newcomer takes the CPU.
ScheduleResult priorityPreemptive(ProcessList& list);

// Convenience wrapper used by the server's "preemptive" toggle.
ScheduleResult priorityScheduling(ProcessList& list, bool preemptive);
