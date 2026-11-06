#pragma once
#include <string>
#include "process.h"
#include "scheduler.h"
#include "stats.h"

struct RunInfo {
    std::string algorithm;
    int quantum = 0;
    bool preemptive = false;
};

std::string serializeResult(const ProcessList& list, const ScheduleResult& result,const Stats& stats, const RunInfo& info);