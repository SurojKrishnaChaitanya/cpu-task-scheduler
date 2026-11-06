#pragma once
#include <string>

struct Process {
    int pid;
    int arrival;
    int burst;     
    int priority;   
    int remaining;   
    int start;     
    int completion; 
    int turnaround;
    int waiting;
    Process* next;
    Process(int pid, int arrival, int burst, int priority);
};

class ProcessList {
public:
    ProcessList() = default;
    ~ProcessList();
    ProcessList(const ProcessList&) = delete;
    ProcessList& operator=(const ProcessList&) = delete;

    Process* append(int pid, int arrival, int burst, int priority,
                    std::string* error = nullptr);

    Process* findByPid(int pid) const;

    void resetResults();

    Process* head() const { return head_; }
    int size() const { return size_; }

private:
    Process* head_ = nullptr;
    Process* tail_ = nullptr;
    int size_ = 0;
};