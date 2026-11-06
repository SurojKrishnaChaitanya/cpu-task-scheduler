#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "circular_queue.h"
#include "heap.h"
#include "process.h"
#include "scheduler.h"
#include "stats.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("  FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)
static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

// Compares the Gantt log with an expected list of {pid,start,end}.
static bool ganttIs(const ScheduleResult& r, const std::vector<GanttEntry>& want) {
    if (r.gantt.size() != want.size()) return false;
    for (size_t i = 0; i < want.size(); ++i)
        if (r.gantt[i].pid != want[i].pid || r.gantt[i].start != want[i].start ||
            r.gantt[i].end != want[i].end) return false;
    return true;
}

static void printRun(const ProcessList& list, const ScheduleResult& r, const Stats& s) {
    std::printf("  Gantt: ");
    for (const auto& g : r.gantt) {
        if (g.pid == IDLE_PID) std::printf("[idle %d-%d] ", g.start, g.end);
        else                   std::printf("[P%d %d-%d] ", g.pid, g.start, g.end);
    }
    std::printf("\n  PID  AT  BT  CT  TAT  WT\n");
    for (Process* p = list.head(); p; p = p->next)
        std::printf("  P%-3d %2d  %2d  %2d  %3d  %2d\n", p->pid, p->arrival,
                    p->burst, p->completion, p->turnaround, p->waiting);
    std::printf("  avg TAT = %.2f, avg WT = %.2f, total = %d, idle = %d\n",
                s.avgTAT, s.avgWT, r.totalTime, r.idleTime);
}

// ---------------- Stage 1 tests ----------------
static void testFormulasOnWorkedExample() {
    std::printf("Test: formulas on the hand-worked RR example\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 1, 3, 1);
    list.append(3, 2, 1, 1);
    list.findByPid(1)->completion = 9;
    list.findByPid(2)->completion = 8;
    list.findByPid(3)->completion = 5;
    Stats s = computeStats(list);
    CHECK(list.findByPid(1)->turnaround == 9 && list.findByPid(1)->waiting == 4);
    CHECK(list.findByPid(2)->turnaround == 7 && list.findByPid(2)->waiting == 4);
    CHECK(list.findByPid(3)->turnaround == 3 && list.findByPid(3)->waiting == 2);
    CHECK(s.count == 3);
    CHECK(near(s.avgTAT, 19.0 / 3));
    CHECK(near(s.avgWT, 10.0 / 3));
}

static void testValidation() {
    std::printf("Test: validation\n");
    ProcessList list;
    std::string err;
    CHECK(list.append(1, 0, 5, 2, &err) != nullptr);
    CHECK(list.append(1, 3, 4, 1, &err) == nullptr && err == "Duplicate PID");
    CHECK(list.append(2, -1, 4, 1, &err) == nullptr);
    CHECK(list.append(3, 0, 0, 1, &err) == nullptr);
    CHECK(list.append(4, 0, -2, 1, &err) == nullptr);
    CHECK(list.size() == 1);
}

static void testResetAndFind() {
    std::printf("Test: reset and find\n");
    ProcessList list;
    list.append(7, 0, 4, 3);
    list.append(9, 2, 6, 1);
    Process* p = list.findByPid(9);
    CHECK(p && p->burst == 6);
    CHECK(list.findByPid(42) == nullptr);
    p->remaining = 0; p->completion = 20; p->start = 5;
    list.resetResults();
    CHECK(p->remaining == 6 && p->completion == 0 && p->start == -1);
}

static void testEmptyList() {
    std::printf("Test: empty list\n");
    ProcessList list;
    Stats s = computeStats(list);
    CHECK(s.count == 0 && near(s.avgWT, 0) && near(s.avgTAT, 0));
}

// ---------------- Stage 2 tests ----------------
static void testCircularQueue() {
    std::printf("Test: circular queue wrap-around\n");
    Process a(1,0,1,1), b(2,0,1,1), c(3,0,1,1), d(4,0,1,1);
    CircularQueue q(3);
    CHECK(q.empty());
    CHECK(q.enqueue(&a) && q.enqueue(&b) && q.enqueue(&c));
    CHECK(q.full() && !q.enqueue(&d));          // full: rejected
    CHECK(q.dequeue() == &a);
    CHECK(q.enqueue(&d));                       // reuses the freed slot (wraps)
    CHECK(q.dequeue() == &b);
    CHECK(q.dequeue() == &c);
    CHECK(q.dequeue() == &d);                   // FIFO order preserved across the wrap
    CHECK(q.empty() && q.dequeue() == nullptr);
}

static void testRoundRobinBasic() {
    std::printf("Test: RR basic (P1 0/5, P2 1/3, P3 2/1, q=2)\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 1, 3, 1);
    list.append(3, 2, 1, 1);
    ScheduleResult r = roundRobin(list, 2);
    Stats s = computeStats(list);
    printRun(list, r, s);
    CHECK(r.ok());
    CHECK(ganttIs(r, {{1,0,2},{2,2,4},{3,4,5},{1,5,7},{2,7,8},{1,8,9}}));
    CHECK(list.findByPid(1)->completion == 9);
    CHECK(list.findByPid(2)->completion == 8);
    CHECK(list.findByPid(3)->completion == 5);
    CHECK(list.findByPid(1)->waiting == 4 && list.findByPid(2)->waiting == 4 &&
          list.findByPid(3)->waiting == 2);
    CHECK(near(s.avgTAT, 19.0 / 3) && near(s.avgWT, 10.0 / 3));
    CHECK(r.totalTime == 9 && r.idleTime == 0);
}

static void testIdleGapMiddle() {
    std::printf("Test: idle gap in the middle (P1 0/2, P2 10/2, q=2)\n");
    ProcessList list;
    list.append(1, 0, 2, 1);
    list.append(2, 10, 2, 1);
    ScheduleResult r = roundRobin(list, 2);
    Stats s = computeStats(list);
    printRun(list, r, s);
    CHECK(ganttIs(r, {{1,0,2},{IDLE_PID,2,10},{2,10,12}}));
    CHECK(r.idleTime == 8 && r.totalTime == 12);
    CHECK(list.findByPid(2)->waiting == 0);
}

static void testIdleStart() {
    std::printf("Test: idle at start (nobody arrives at 0)\n");
    ProcessList list;
    list.append(1, 3, 2, 1);
    ScheduleResult r = roundRobin(list, 4);
    computeStats(list);
    CHECK(ganttIs(r, {{IDLE_PID,0,3},{1,3,5}}));
    CHECK(r.idleTime == 3 && list.findByPid(1)->completion == 5);
}

static void testQuantumLargerThanBursts() {
    std::printf("Test: huge quantum behaves like FCFS\n");
    ProcessList list;
    list.append(1, 0, 3, 1);
    list.append(2, 1, 2, 1);
    list.append(3, 2, 1, 1);
    ScheduleResult r = roundRobin(list, 100);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,3},{2,3,5},{3,5,6}}));
    CHECK(list.findByPid(1)->completion == 3 && list.findByPid(2)->completion == 5 &&
          list.findByPid(3)->completion == 6);
}

static void testQuantumOne() {
    std::printf("Test: quantum 1 (hand-worked)\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 1, 3, 1);
    list.append(3, 2, 1, 1);
    ScheduleResult r = roundRobin(list, 1);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,1},{2,1,2},{1,2,3},{3,3,4},{2,4,5},{1,5,6},{2,6,7},{1,7,9}}));
    CHECK(list.findByPid(1)->completion == 9);
    CHECK(list.findByPid(2)->completion == 7);
    CHECK(list.findByPid(3)->completion == 4);
}

static void testArrivalWhenAnotherFinishes() {
    std::printf("Test: arrival exactly when another finishes (no fake idle)\n");
    ProcessList list;
    list.append(1, 0, 2, 1);
    list.append(2, 2, 2, 1);
    ScheduleResult r = roundRobin(list, 2);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,2},{2,2,4}}));
    CHECK(r.idleTime == 0);
}

static void testSameArrivalTieBreak() {
    std::printf("Test: same arrival time -> lower PID first\n");
    ProcessList list;
    list.append(2, 0, 2, 1);   // entered first but has the higher PID
    list.append(1, 0, 2, 1);
    ScheduleResult r = roundRobin(list, 2);
    CHECK(ganttIs(r, {{1,0,2},{2,2,4}}));
}

static void testRerunAndInvalid() {
    std::printf("Test: re-run gives identical results; invalid quantum\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 1, 3, 1);
    ScheduleResult r1 = roundRobin(list, 2);
    int ct1 = list.findByPid(1)->completion;
    ScheduleResult r2 = roundRobin(list, 2);
    CHECK(r1.gantt.size() == r2.gantt.size());
    CHECK(list.findByPid(1)->completion == ct1);
    ScheduleResult bad = roundRobin(list, 0);
    CHECK(!bad.ok());
    ProcessList empty;
    CHECK(roundRobin(empty, 2).ok());
}

// ---------------- Stage 3 tests ----------------
static void testComparisonRule() {
    std::printf("Test: comparison rule (priority, then arrival, then PID)\n");
    Process hiPri(5, 9, 1, 8), loPri(1, 0, 1, 2);
    CHECK(hasHigherPriority(&hiPri, &loPri));          // bigger number wins
    CHECK(!hasHigherPriority(&loPri, &hiPri));
    Process early(5, 1, 1, 4), late(2, 3, 1, 4);
    CHECK(hasHigherPriority(&early, &late));           // same priority: earlier arrival
    Process lowPid(1, 2, 1, 4), highPid(7, 2, 1, 4);
    CHECK(hasHigherPriority(&lowPid, &highPid));       // same both: lower PID
    CHECK(!hasHigherPriority(&lowPid, &lowPid));       // irreflexive
}

static void testHeapBasics() {
    std::printf("Test: heap insert / extract-top order\n");
    Process a(1,0,1,3), b(2,0,1,7), c(3,0,1,5), d(4,0,1,7), e(5,0,1,1);
    Heap h;
    CHECK(h.empty() && h.extractTop() == nullptr && h.peek() == nullptr);
    h.insert(&a); h.insert(&b); h.insert(&c); h.insert(&d); h.insert(&e);
    CHECK(h.size() == 5);
    CHECK(h.peek() == &b);                 // priority 7, lower PID than d
    CHECK(h.extractTop() == &b);
    CHECK(h.extractTop() == &d);           // the other priority 7
    CHECK(h.extractTop() == &c);           // 5
    CHECK(h.extractTop() == &a);           // 3
    CHECK(h.extractTop() == &e);           // 1
    CHECK(h.empty());
}

static void testHeapManyItems() {
    std::printf("Test: heap with 200 pseudo-random items comes out fully sorted\n");
    std::vector<Process*> items;
    unsigned seed = 12345;
    for (int i = 0; i < 200; ++i) {
        seed = seed * 1103515245u + 12345u;
        int pri = static_cast<int>((seed >> 16) % 10);
        seed = seed * 1103515245u + 12345u;
        int at  = static_cast<int>((seed >> 16) % 20);
        items.push_back(new Process(i, at, 1, pri));
    }
    Heap h;
    for (Process* p : items) h.insert(p);
    bool sorted = true;
    Process* prev = h.extractTop();
    int count = 1;
    while (!h.empty()) {
        Process* cur = h.extractTop();
        if (hasHigherPriority(cur, prev)) sorted = false;   // must never beat the previous one
        prev = cur;
        ++count;
    }
    CHECK(sorted);
    CHECK(count == 200);
    for (Process* p : items) delete p;
}

static void testPriorityBasic() {
    std::printf("Test: priority NP basic (hand-worked)\n");
    ProcessList list;
    list.append(1, 0, 4, 2);
    list.append(2, 1, 3, 4);
    list.append(3, 2, 1, 3);
    list.append(4, 3, 2, 1);
    ScheduleResult r = priorityNonPreemptive(list);
    Stats s = computeStats(list);
    printRun(list, r, s);
    CHECK(r.ok());
    // P1 is alone at time 0 and is NOT interrupted by higher-priority P2.
    CHECK(ganttIs(r, {{1,0,4},{2,4,7},{3,7,8},{4,8,10}}));
    CHECK(list.findByPid(1)->completion == 4 && list.findByPid(2)->completion == 7 &&
          list.findByPid(3)->completion == 8 && list.findByPid(4)->completion == 10);
    CHECK(list.findByPid(1)->waiting == 0 && list.findByPid(2)->waiting == 3 &&
          list.findByPid(3)->waiting == 5 && list.findByPid(4)->waiting == 5);
    CHECK(near(s.avgTAT, 23.0 / 4) && near(s.avgWT, 13.0 / 4));
}

static void testPriorityTieEarlierArrival() {
    std::printf("Test: equal priority -> earlier arrival first (not lower PID)\n");
    ProcessList list;
    list.append(1, 0, 3, 1);
    list.append(2, 2, 2, 5);   // lower PID but arrives later
    list.append(3, 1, 2, 5);   // higher PID but arrives earlier
    ScheduleResult r = priorityNonPreemptive(list);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,3},{3,3,5},{2,5,7}}));
}

static void testPriorityTieSamePid() {
    std::printf("Test: equal priority and arrival -> lower PID first\n");
    ProcessList list;
    list.append(2, 0, 2, 3);   // entered first but higher PID
    list.append(1, 0, 2, 3);
    ScheduleResult r = priorityNonPreemptive(list);
    CHECK(ganttIs(r, {{1,0,2},{2,2,4}}));
}

static void testPriorityHigherNumberWins() {
    std::printf("Test: higher number = higher priority (all arrive at 0)\n");
    ProcessList list;
    list.append(1, 0, 1, 1);
    list.append(2, 0, 1, 9);
    list.append(3, 0, 1, 5);
    ScheduleResult r = priorityNonPreemptive(list);
    CHECK(ganttIs(r, {{2,0,1},{3,1,2},{1,2,3}}));
}

static void testPriorityIdle() {
    std::printf("Test: priority idle gaps (start and middle)\n");
    ProcessList list;
    list.append(1, 3, 2, 1);
    list.append(2, 10, 2, 1);
    ScheduleResult r = priorityNonPreemptive(list);
    computeStats(list);
    CHECK(ganttIs(r, {{IDLE_PID,0,3},{1,3,5},{IDLE_PID,5,10},{2,10,12}}));
    CHECK(r.idleTime == 8 && r.totalTime == 12);
}

static void testPriorityArrivalAtFinish() {
    std::printf("Test: arrival exactly when running process finishes (no fake idle)\n");
    ProcessList list;
    list.append(1, 0, 2, 1);
    list.append(2, 2, 2, 9);
    ScheduleResult r = priorityNonPreemptive(list);
    CHECK(ganttIs(r, {{1,0,2},{2,2,4}}));
    CHECK(r.idleTime == 0);
}

static void testSwitchingAlgorithmsOnSameList() {
    std::printf("Test: same list under RR then Priority gives independent results\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 1, 3, 2);
    list.append(3, 2, 1, 3);
    roundRobin(list, 2);
    int rrCt1 = list.findByPid(1)->completion;          // 9 from the RR hand example
    ScheduleResult r = priorityNonPreemptive(list);
    CHECK(rrCt1 == 9);
    CHECK(ganttIs(r, {{1,0,5},{3,5,6},{2,6,9}}));
    CHECK(list.findByPid(1)->completion == 5 && list.findByPid(2)->completion == 9 &&
          list.findByPid(3)->completion == 6);
    ScheduleResult again = roundRobin(list, 2);          // back to RR: same as before
    CHECK(list.findByPid(1)->completion == 9 && again.gantt.size() == 6);
}

static void testPrioritySingleAndEmpty() {
    std::printf("Test: priority single process and empty list\n");
    ProcessList one;
    one.append(1, 0, 4, 1);
    ScheduleResult r = priorityNonPreemptive(one);
    CHECK(ganttIs(r, {{1,0,4}}));
    ProcessList empty;
    CHECK(priorityNonPreemptive(empty).ok());
}

// ---------------- Stage 4 tests ----------------
static void testPreemptiveBasic() {
    std::printf("Test: priority preemptive basic (same input as the NP example)\n");
    ProcessList list;
    list.append(1, 0, 4, 2);
    list.append(2, 1, 3, 4);
    list.append(3, 2, 1, 3);
    list.append(4, 3, 2, 1);
    ScheduleResult r = priorityPreemptive(list);
    Stats s = computeStats(list);
    printRun(list, r, s);
    CHECK(r.ok());
    CHECK(ganttIs(r, {{1,0,1},{2,1,4},{3,4,5},{1,5,8},{4,8,10}}));
    CHECK(list.findByPid(1)->completion == 8 && list.findByPid(2)->completion == 4 &&
          list.findByPid(3)->completion == 5 && list.findByPid(4)->completion == 10);
    CHECK(list.findByPid(1)->waiting == 4 && list.findByPid(2)->waiting == 0 &&
          list.findByPid(3)->waiting == 2 && list.findByPid(4)->waiting == 5);
    CHECK(near(s.avgTAT, 21.0 / 4) && near(s.avgWT, 11.0 / 4));
}

static void testPreemptionSwitchesImmediately() {
    std::printf("Test: low-priority running, high-priority arrives -> switch at once\n");
    ProcessList list;
    list.append(1, 0, 5, 1);
    list.append(2, 2, 2, 5);
    ScheduleResult r = priorityPreemptive(list);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,2},{2,2,4},{1,4,7}}));
    CHECK(list.findByPid(2)->completion == 4 && list.findByPid(1)->completion == 7);
    CHECK(list.findByPid(1)->start == 0);   // start time is NOT overwritten on resume
    CHECK(list.findByPid(2)->start == 2);
}

static void testEqualPriorityDoesNotPreempt() {
    std::printf("Test: equal priority never preempts (running one arrived earlier)\n");
    ProcessList list;
    list.append(1, 0, 4, 3);
    list.append(2, 1, 2, 3);
    ScheduleResult r = priorityPreemptive(list);
    CHECK(ganttIs(r, {{1,0,4},{2,4,6}}));
}

static void testLowerPriorityDoesNotPreempt() {
    std::printf("Test: lower-priority arrival waits\n");
    ProcessList list;
    list.append(1, 0, 4, 5);
    list.append(2, 1, 1, 2);
    ScheduleResult r = priorityPreemptive(list);
    CHECK(ganttIs(r, {{1,0,4},{2,4,5}}));
}

static void testChainOfPreemptions() {
    std::printf("Test: chain of preemptions, remaining time carried over\n");
    ProcessList list;
    list.append(1, 0, 6, 1);
    list.append(2, 1, 4, 2);
    list.append(3, 2, 2, 3);
    ScheduleResult r = priorityPreemptive(list);
    computeStats(list);
    CHECK(ganttIs(r, {{1,0,1},{2,1,2},{3,2,4},{2,4,7},{1,7,12}}));
    CHECK(list.findByPid(1)->completion == 12 && list.findByPid(2)->completion == 7 &&
          list.findByPid(3)->completion == 4);
}

static void testPreemptiveIdleAndBoundaries() {
    std::printf("Test: preemptive idle gaps and arrival exactly at finish\n");
    ProcessList list;
    list.append(1, 3, 2, 1);
    list.append(2, 5, 2, 9);    // arrives exactly when P1 finishes
    list.append(3, 12, 1, 1);
    ScheduleResult r = priorityPreemptive(list);
    computeStats(list);
    CHECK(ganttIs(r, {{IDLE_PID,0,3},{1,3,5},{2,5,7},{IDLE_PID,7,12},{3,12,13}}));
    CHECK(r.idleTime == 8 && r.totalTime == 13);
}

static void testPreemptiveEqualsNPWhenAllArriveTogether() {
    std::printf("Test: all arrive at 0 -> preemptive and non-preemptive agree\n");
    ProcessList a, b;
    int bt[] = {3, 2, 4, 1}, pr[] = {2, 5, 5, 1};
    for (int i = 0; i < 4; ++i) { a.append(i + 1, 0, bt[i], pr[i]); b.append(i + 1, 0, bt[i], pr[i]); }
    ScheduleResult np = priorityNonPreemptive(a);
    ScheduleResult p  = priorityPreemptive(b);
    CHECK(np.gantt.size() == p.gantt.size());
    bool same = np.gantt.size() == p.gantt.size();
    for (size_t i = 0; same && i < np.gantt.size(); ++i)
        same = np.gantt[i].pid == p.gantt[i].pid && np.gantt[i].start == p.gantt[i].start &&
               np.gantt[i].end == p.gantt[i].end;
    CHECK(same);
}

static void testWrapperAndSwitching() {
    std::printf("Test: wrapper toggle, and NP/preemptive on the same list\n");
    ProcessList list;
    list.append(1, 0, 4, 2);
    list.append(2, 1, 3, 4);
    list.append(3, 2, 1, 3);
    list.append(4, 3, 2, 1);
    ScheduleResult np = priorityScheduling(list, false);
    CHECK(list.findByPid(1)->completion == 4 && np.gantt.size() == 4);
    ScheduleResult pr = priorityScheduling(list, true);
    CHECK(list.findByPid(1)->completion == 8 && pr.gantt.size() == 5);
    ScheduleResult np2 = priorityScheduling(list, false);
    CHECK(list.findByPid(1)->completion == 4 && np2.gantt.size() == 4);
}

static void testPreemptiveSingleAndEmpty() {
    std::printf("Test: preemptive single process and empty list\n");
    ProcessList one;
    one.append(1, 2, 4, 1);
    ScheduleResult r = priorityPreemptive(one);
    CHECK(ganttIs(r, {{IDLE_PID,0,2},{1,2,6}}));
    ProcessList empty;
    CHECK(priorityPreemptive(empty).ok());
}

// ---------------- Stage 7 tests (queue snapshots for the animation) ----------------
static bool queuesAre(const ScheduleResult& r, const std::vector<std::vector<int>>& want) {
    if (r.gantt.size() != want.size()) return false;
    for (size_t i = 0; i < want.size(); ++i)
        if (r.gantt[i].queue != want[i]) return false;
    return true;
}

static void testQueueSnapshots() {
    std::printf("Test: ready-queue snapshots (RR circular queue, priority heap)\n");
    ProcessList rr;
    rr.append(1, 0, 5, 1); rr.append(2, 1, 3, 1); rr.append(3, 2, 1, 1);
    ScheduleResult a = roundRobin(rr, 2);
    // Waiting list at the start of each slice, next-to-run first.
    CHECK(queuesAre(a, {{}, {3,1}, {1,2}, {2}, {1}, {}}));

    ProcessList pr;
    pr.append(1, 0, 4, 2); pr.append(2, 1, 3, 4); pr.append(3, 2, 1, 3); pr.append(4, 3, 2, 1);
    ScheduleResult b = priorityNonPreemptive(pr);
    CHECK(queuesAre(b, {{}, {3,4}, {4}, {}}));       // heap order: P3 (3) before P4 (1)

    ProcessList idle;
    idle.append(1, 0, 2, 1); idle.append(2, 10, 2, 1);
    ScheduleResult c = roundRobin(idle, 2);
    CHECK(queuesAre(c, {{}, {}, {}}));               // idle gap has an empty queue
}

// ---------------- Stage 8 test: adding a process "now" never rewrites the past ----------------
// The live "add a process" feature relies on this: re-running with a new process
// that arrives at or after the current clock must give the same schedule before it.
static int pidAt(const ScheduleResult& r, int t) {
    for (const auto& g : r.gantt) if (g.start <= t && t < g.end) return g.pid;
    return -2;   // beyond the end of the schedule
}

static void testPrefixStability() {
    std::printf("Test: adding a process at clock >= now never changes what happened before\n");
    unsigned seed = 2024;
    auto rnd = [&](int n) { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) % n); };
    int mismatches = 0, completionChanges = 0, counts[3] = {0, 0, 0};

    for (int trial = 0; trial < 3000; ++trial) {
        int n = 1 + rnd(6), algo = rnd(3), quantum = 1 + rnd(4);   // 0 = RR, 1 = priority NP, 2 = preemptive
        ProcessList before, after;
        for (int i = 0; i < n; ++i) {
            int at = rnd(10), bt = 1 + rnd(6), pr = rnd(5);
            before.append(i + 1, at, bt, pr);
            after.append(i + 1, at, bt, pr);
        }
        auto runAlgo = [&](ProcessList& l) {
            return algo == 0 ? roundRobin(l, quantum) : priorityScheduling(l, algo == 2);
        };
        ScheduleResult a = runAlgo(before);

        int now = rnd(a.totalTime + 1);          // a moment during (or at the end of) the run
        int arrival = now + rnd(3);              // the new process arrives now or later
        after.append(n + 1, arrival, 1 + rnd(6), rnd(5));
        ScheduleResult b = runAlgo(after);

        int limit = arrival < a.totalTime ? arrival : a.totalTime;
        for (int t = 0; t < limit; ++t)
            if (pidAt(a, t) != pidAt(b, t)) ++mismatches;
        for (Process* p = before.head(); p; p = p->next)       // anything already finished stays finished
            if (p->completion <= limit && after.findByPid(p->pid)->completion != p->completion)
                ++completionChanges;
        ++counts[algo];
    }
    std::printf("  3000 random trials (RR %d, priority %d, preemptive %d)\n", counts[0], counts[1], counts[2]);
    CHECK(mismatches == 0);
    CHECK(completionChanges == 0);
}

// ---------------- Stage 9 test: response time ----------------
static void testResponseTimes() {
    std::printf("Test: response time (first time on the CPU minus arrival)\n");
    ProcessList rr;
    rr.append(1, 0, 5, 1); rr.append(2, 1, 3, 1); rr.append(3, 2, 1, 1);
    roundRobin(rr, 2);
    Stats s = computeStats(rr);
    CHECK(rr.findByPid(1)->start == 0 && rr.findByPid(2)->start == 2 && rr.findByPid(3)->start == 4);
    CHECK(near(s.avgRT, 1.0));                          // (0 + 1 + 2) / 3

    ProcessList np, pre;
    for (ProcessList* l : {&np, &pre}) {
        l->append(1, 0, 4, 2); l->append(2, 1, 3, 4); l->append(3, 2, 1, 3); l->append(4, 3, 2, 1);
    }
    priorityNonPreemptive(np);
    CHECK(near(computeStats(np).avgRT, 13.0 / 4));      // (0 + 3 + 5 + 5) / 4
    priorityPreemptive(pre);
    CHECK(near(computeStats(pre).avgRT, 7.0 / 4));      // (0 + 0 + 2 + 5) / 4
}

int main() {
    testFormulasOnWorkedExample();
    testValidation();
    testResetAndFind();
    testEmptyList();
    testCircularQueue();
    testRoundRobinBasic();
    testIdleGapMiddle();
    testIdleStart();
    testQuantumLargerThanBursts();
    testQuantumOne();
    testArrivalWhenAnotherFinishes();
    testSameArrivalTieBreak();
    testRerunAndInvalid();
    testComparisonRule();
    testHeapBasics();
    testHeapManyItems();
    testPriorityBasic();
    testPriorityTieEarlierArrival();
    testPriorityTieSamePid();
    testPriorityHigherNumberWins();
    testPriorityIdle();
    testPriorityArrivalAtFinish();
    testSwitchingAlgorithmsOnSameList();
    testPrioritySingleAndEmpty();
    testPreemptiveBasic();
    testPreemptionSwitchesImmediately();
    testEqualPriorityDoesNotPreempt();
    testLowerPriorityDoesNotPreempt();
    testChainOfPreemptions();
    testPreemptiveIdleAndBoundaries();
    testPreemptiveEqualsNPWhenAllArriveTogether();
    testWrapperAndSwitching();
    testPreemptiveSingleAndEmpty();
    testQueueSnapshots();
    testPrefixStability();
    testResponseTimes();
    if (failures == 0) std::printf("\nAll tests passed (Stages 1-4).\n");
    else std::printf("\n%d check(s) FAILED.\n", failures);
    return failures ? 1 : 0;
}
