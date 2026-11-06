#include "stats.h"

Stats computeStats(ProcessList& list) {
    Stats s;
    for (Process* p = list.head(); p; p = p->next) {
        p->turnaround = turnaroundTime(p->completion, p->arrival);
        p->waiting    = waitingTime(p->turnaround, p->burst);
        s.totalTAT += p->turnaround;
        s.totalWT  += p->waiting;
        s.totalRT  += (p->start >= 0) ? p->start - p->arrival : 0;
        ++s.count;
    }
    if (s.count > 0) {
        s.avgTAT = s.totalTAT / s.count;
        s.avgWT  = s.totalWT  / s.count;
        s.avgRT  = s.totalRT  / s.count;
    }
    return s;
}
