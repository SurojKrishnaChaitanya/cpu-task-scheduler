#pragma once
#include <string>

struct Process {
    int pid;
    int arrival;    // AT
    int burst;      // BT
    int priority;   // HIGHER number = MORE urgent (project decision)
    int remaining;   // starts at burst, counts down while running
    int start;       // first time on CPU, -1 = not started yet
    int completion;  // CT
    int turnaround;  // TAT = CT - AT
    int waiting;     // WT  = TAT - BT

    Process* next;   // linked-list link

    Process(int pid, int arrival, int burst, int priority);
};

class ProcessList {
public:
    ProcessList() = default;
    ~ProcessList();

    ProcessList(const ProcessList&) = delete;
    ProcessList& operator=(const ProcessList&) = delete;

    // Validates and appends. Returns the new node, or nullptr on invalid input
    // (error text goes into *error if provided).
    Process* append(int pid, int arrival, int burst, int priority,
                    std::string* error = nullptr);

    Process* findByPid(int pid) const;

    // Sets remaining = burst, start = -1, and clears CT/TAT/WT.
    // Call before every simulation so the same input can be re-run.
    void resetResults();

    Process* head() const { return head_; }
    int size() const { return size_; }

private:
    Process* head_ = nullptr;
    Process* tail_ = nullptr;
    int size_ = 0;
};
