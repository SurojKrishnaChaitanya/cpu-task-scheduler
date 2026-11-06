#include "scheduler.h"
#include <algorithm>
#include "circular_queue.h"
#include "heap.h"
using namespace std;

static void addSlice(vector<GanttEntry>& gantt, int pid, int start, int end,
                     vector<int> queue = {}) {
    if (!gantt.empty() && gantt.back().pid == pid && gantt.back().end == start)
        gantt.back().end = end;
    else
        gantt.push_back(GanttEntry(pid, start, end, move(queue)));
}

static vector<Process*> sortedByArrival(const ProcessList& list) {
    vector<Process*> v;
    for (Process* p = list.head(); p; p = p->next) v.push_back(p);
    sort(v.begin(), v.end(), [](const Process* a, const Process* b) {
        if (a->arrival != b->arrival) return a->arrival < b->arrival;
        return a->pid < b->pid;
    });
    return v;
}

ScheduleResult roundRobin(ProcessList& list, int quantum) {
    ScheduleResult result;
    if (quantum < 1) { result.error = "Time quantum must be at least 1"; return result; }
    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;
    vector<Process*> byArrival = sortedByArrival(list);
    CircularQueue ready(n);
    int clock = 0;
    int nextArrival = 0;
    int completed = 0;
    auto admitArrivals = [&]() {
        while (nextArrival < n && byArrival[nextArrival]->arrival <= clock)
            ready.enqueue(byArrival[nextArrival++]);
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
        Process* p = ready.dequeue();
        if (p->start == -1) p->start = clock;
        int run = min(quantum, p->remaining);
        addSlice(result.gantt, p->pid, clock, clock + run, ready.pids());
        clock += run;
        p->remaining -= run;
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

ScheduleResult priorityNonPreemptive(ProcessList& list) {
    ScheduleResult result;
    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;
    vector<Process*> byArrival = sortedByArrival(list);
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

ScheduleResult priorityPreemptive(ProcessList& list) {
    ScheduleResult result;
    list.resetResults();
    const int n = list.size();
    if (n == 0) return result;
    vector<Process*> byArrival = sortedByArrival(list);
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
        Process* p = ready.extractTop();
        if (p->start == -1) p->start = clock;
        int run = p->remaining;
        if (nextArrival < n) {
            int untilArrival = byArrival[nextArrival]->arrival - clock;
            if (untilArrival < run) run = untilArrival;
        }

        addSlice(result.gantt, p->pid, clock, clock + run, ready.orderedPids());
        clock += run;
        p->remaining -= run;
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