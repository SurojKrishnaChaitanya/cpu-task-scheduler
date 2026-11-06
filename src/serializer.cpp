#include "serializer.h"
#include <iomanip>
#include <sstream>
#include "json_mini.h"
using namespace std;
using namespace mj;

string serializeResult(const ProcessList& list, const ScheduleResult& result,
                            const Stats& stats, const RunInfo& info) {
    ostringstream o;
    o << setprecision(15);

    o << "{\"algorithm\":" << quote(info.algorithm);
    if (info.algorithm == "rr") o << ",\"quantum\":" << info.quantum;
    else o << ",\"preemptive\":" << (info.preemptive ? "true" : "false");

    o << ",\"gantt\":[";
    for (size_t k = 0; k < result.gantt.size(); ++k) {
        const GanttEntry& g = result.gantt[k];
        bool idle = (g.pid == IDLE_PID);
        if (k) o << ',';
        o << "{\"pid\":";
        if (idle) o << "null"; else o << g.pid;
        o << ",\"start\":" << g.start << ",\"end\":" << g.end
          << ",\"idle\":" << (idle ? "true" : "false") << ",\"queue\":[";
        for (size_t q = 0; q < g.queue.size(); ++q) o << (q ? "," : "") << g.queue[q];
        o << "]}";
    }

    o << "],\"processes\":[";
    bool first = true;
    for (const Process* p = list.head(); p; p = p->next) {
        if (!first) o << ',';
        first = false;
        o << "{\"pid\":" << p->pid << ",\"arrival\":" << p->arrival
          << ",\"burst\":" << p->burst << ",\"priority\":" << p->priority
          << ",\"completion\":" << p->completion << ",\"turnaround\":" << p->turnaround
          << ",\"waiting\":" << p->waiting
          << ",\"response\":" << ((p->start >= 0) ? p->start - p->arrival : 0) << "}";
    }

    o << "],\"averageWaiting\":" << stats.avgWT
      << ",\"averageTurnaround\":" << stats.avgTAT
      << ",\"averageResponse\":" << stats.avgRT
      << ",\"totalTime\":" << result.totalTime
      << ",\"idleTime\":" << result.idleTime << "}";
    return o.str();
}