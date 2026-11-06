#include "process.h"

Process::Process(int pid, int arrival, int burst, int priority)
    : pid(pid), arrival(arrival), burst(burst), priority(priority),
      remaining(burst), start(-1), completion(0), turnaround(0), waiting(0),
      next(nullptr) {}

ProcessList::~ProcessList() {
    Process* cur = head_;
    while (cur) {
        Process* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
}

Process* ProcessList::append(int pid, int arrival, int burst, int priority,
                            std::string* error) {
    auto fail = [&](const char* msg) -> Process* {
        if (error) *error = msg;
        return nullptr;
    };

    if (pid < 0)      return fail("PID must not be negative");
    if (arrival < 0)  return fail("Arrival time must not be negative");
    if (burst <= 0)   return fail("Burst time must be at least 1");
    if (findByPid(pid)) return fail("Duplicate PID");

    Process* node = new Process(pid, arrival, burst, priority);
    if (!head_) head_ = tail_ = node;
    else { tail_->next = node; tail_ = node; }
    ++size_;
    return node;
}

Process* ProcessList::findByPid(int pid) const {
    for (Process* p = head_; p; p = p->next)
        if (p->pid == pid) return p;
    return nullptr;
}

void ProcessList::resetResults() {
    for (Process* p = head_; p; p = p->next) {
        p->remaining  = p->burst;
        p->start      = -1;
        p->completion = 0;
        p->turnaround = 0;
        p->waiting    = 0;
    }
}
