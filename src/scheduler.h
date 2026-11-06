#pragma once
#include <string>
#include <utility>
#include <vector>
#include "process.h"
constexpr int IDLE_PID = -1;

struct GanttEntry {
    int pid;
    int start;
    int end;
    std::vector<int> queue;

    GanttEntry(int p, int s, int e, std::vector<int> q = {})
        : pid(p), start(s), end(e), queue(std::move(q)) {}
};

struct ScheduleResult {
    std::vector<GanttEntry> gantt;
    int totalTime = 0;
    int idleTime  = 0;
    std::string error;   
    bool ok() const { return error.empty(); }
};

ScheduleResult roundRobin(ProcessList& list, int quantum);
ScheduleResult priorityNonPreemptive(ProcessList& list);
ScheduleResult priorityPreemptive(ProcessList& list);
ScheduleResult priorityScheduling(ProcessList& list, bool preemptive);