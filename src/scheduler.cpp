#include "scheduler.h"
#include <algorithm>
#include "circular_queue.h"
#include "heap.h"

// ---------------------------------------------------------------- helpers

// Appends a slice to the Gantt log, merging with the previous entry when the
// same process simply continues (e.g. P1 [7-8] followed by P1 [8-9]).
static void addSlice(std::vector<GanttEntry>& gantt, int pid, int start, int end,
                     std::vector<int> queue = {}) {
    if (!gantt.empty() && gantt.back().pid == pid && gantt.back().end == start)
        gantt.back().end = end;
    else
        gantt.push_back(GanttEntry(pid, start, end, std::move(queue)));
}

// Sorted view of the processes by (arrival, pid). A vector of pointers into
// the list, so remaining-time updates stay in one place.
static std::vector<Process*> sortedByArrival(const ProcessList& list) {
    std::vector<Process*> v;
    for (Process* p = list.head(); p; p = p->next) v.push_back(p);
    std::sort(v.begin(), v.end(), [](const Process* a, const Process* b) {
        if (a->arrival != b->arrival) return a->arrival < b->arrival;
        return a->pid < b->pid;
    });
    return v;
}

// ------------------------------------------------------------ Round Robin

ScheduleResult roundRobin(ProcessList& list, int quantum) {
    ScheduleResult result;
    if (quantum < 1) { result.error = "Time quantum must be at least 1"; return result; }

    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;

    std::vector<Process*> byArrival = sortedByArrival(list);

    CircularQueue ready(n);
    int clock = 0;
    int nextArrival = 0;   // everything before this index has been enqueued
    int completed = 0;

    // Enqueue every process that has arrived by the current clock value.
    auto admitArrivals = [&]() {
        while (nextArrival < n && byArrival[nextArrival]->arrival <= clock)
            ready.enqueue(byArrival[nextArrival++]);
    };

    while (completed < n) {
        admitArrivals();

        // Nothing ready, but processes remain: CPU idles until the next arrival.
        if (ready.empty()) {
            int wakeUp = byArrival[nextArrival]->arrival;
            addSlice(result.gantt, IDLE_PID, clock, wakeUp);
            result.idleTime += wakeUp - clock;
            clock = wakeUp;
            continue;   // loop top admits the new arrival(s)
        }

        Process* p = ready.dequeue();
        if (p->start == -1) p->start = clock;

        int run = std::min(quantum, p->remaining);
        addSlice(result.gantt, p->pid, clock, clock + run, ready.pids());
        clock += run;
        p->remaining -= run;

        // ORDER MATTERS: processes that arrived during this slice (or exactly
        // at its end) join the queue BEFORE the preempted process goes back.
        admitArrivals();

        if (p->remaining > 0) {
            ready.enqueue(p);
        } else {
            p->completion = clock;
            ++completed;
        }
    }

    result.totalTime = clock;
    return result;
}

// ------------------------------------------- Priority (non-preemptive)

ScheduleResult priorityNonPreemptive(ProcessList& list) {
    ScheduleResult result;

    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;

    std::vector<Process*> byArrival = sortedByArrival(list);

    Heap ready;
    int clock = 0;
    int nextArrival = 0;
    int completed = 0;

    // Push every process that has arrived by the current clock into the heap.
    auto admitArrivals = [&]() {
        while (nextArrival < n && byArrival[nextArrival]->arrival <= clock)
            ready.insert(byArrival[nextArrival++]);
    };

    while (completed < n) {
        admitArrivals();

        // CPU free but nobody ready: jump the clock to the next arrival.
        if (ready.empty()) {
            int wakeUp = byArrival[nextArrival]->arrival;
            addSlice(result.gantt, IDLE_PID, clock, wakeUp);
            result.idleTime += wakeUp - clock;
            clock = wakeUp;
            continue;
        }

        // CPU free: take the best process and run it to completion. Anything
        // that arrives meanwhile simply waits in the heap.
        Process* p = ready.extractTop();
        p->start = clock;
        addSlice(result.gantt, p->pid, clock, clock + p->burst, ready.orderedPids());
        clock += p->burst;
        p->remaining = 0;
        p->completion = clock;
        ++completed;
    }

    result.totalTime = clock;
    return result;
}

// ------------------------------------------------ Priority (preemptive)

ScheduleResult priorityPreemptive(ProcessList& list) {
    ScheduleResult result;

    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;

    std::vector<Process*> byArrival = sortedByArrival(list);

    Heap ready;
    int clock = 0;
    int nextArrival = 0;
    int completed = 0;

    auto admitArrivals = [&]() {
        while (nextArrival < n && byArrival[nextArrival]->arrival <= clock)
            ready.insert(byArrival[nextArrival++]);
    };

    while (completed < n) {
        admitArrivals();

        if (ready.empty()) {
            int wakeUp = byArrival[nextArrival]->arrival;
            addSlice(result.gantt, IDLE_PID, clock, wakeUp);
            result.idleTime += wakeUp - clock;
            clock = wakeUp;
            continue;
        }

        // Best process right now. It was just taken out of the heap, so while
        // it runs it is the only thing that can be "interrupted".
        Process* p = ready.extractTop();
        if (p->start == -1) p->start = clock;

        // Instead of ticking one unit at a time, jump to the next EVENT:
        // either p finishes, or the next process arrives, whichever is first.
        // admitArrivals() already took everyone with arrival <= clock, so the
        // next arrival is strictly in the future and run >= 1.
        int run = p->remaining;
        if (nextArrival < n) {
            int untilArrival = byArrival[nextArrival]->arrival - clock;
            if (untilArrival < run) run = untilArrival;
        }

        addSlice(result.gantt, p->pid, clock, clock + run, ready.orderedPids());
        clock += run;
        p->remaining -= run;

        // Let the new arrival(s) into the heap, then put p back if unfinished.
        // If a newcomer beats p under hasHigherPriority, it is extracted next
        // (preemption). If not, p is extracted again and addSlice merges the
        // two pieces into one Gantt block.
        admitArrivals();

        if (p->remaining > 0) {
            ready.insert(p);
        } else {
            p->completion = clock;
            ++completed;
        }
    }

    result.totalTime = clock;
    return result;
}

ScheduleResult priorityScheduling(ProcessList& list, bool preemptive) {
    return preemptive ? priorityPreemptive(list) : priorityNonPreemptive(list);
}
