#pragma once
#include <utility>
#include <vector>
#include "process.h"
using namespace std;

// THE comparison rule, in exactly one place:
//   1. higher priority number wins   (higher number = more urgent)
//   2. then earlier arrival wins
//   3. then lower PID wins
// Returns true if a should run before b.
inline bool hasHigherPriority(const Process* a, const Process* b) {
    if (a->priority != b->priority) return a->priority > b->priority;
    if (a->arrival  != b->arrival)  return a->arrival  < b->arrival;
    return a->pid < b->pid;
}

// Binary heap stored in an array. The best process is always at index 0.
//   parent of i : (i - 1) / 2
//   children    : 2i + 1 and 2i + 2
// Holds borrowed Process* pointers into the ProcessList, never copies.
class Heap {
public:
    bool empty() const { return a_.empty(); }
    int  size()  const { return static_cast<int>(a_.size()); }
    Process* peek() const { return a_.empty() ? nullptr : a_[0]; }

    // PIDs in the order they would be extracted (best first). Works on a copy.
    vector<int> orderedPids() const {
        Heap copy = *this;
        vector<int> out;
        while (!copy.empty()) out.push_back(copy.extractTop()->pid);
        return out;
    }

    // Put the new item at the end, then sift it UP while it beats its parent.
    void insert(Process* p) {
        a_.push_back(p);
        siftUp(a_.size() - 1);
    }

    // Take index 0, move the last item to the top, then sift it DOWN.
    Process* extractTop() {
        if (a_.empty()) return nullptr;
        Process* top = a_[0];
        a_[0] = a_.back();
        a_.pop_back();
        if (!a_.empty()) siftDown(0);
        return top;
    }

private:
    vector<Process*> a_;

    void siftUp(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!hasHigherPriority(a_[i], a_[parent])) break;
            swap(a_[i], a_[parent]);
            i = parent;
        }
    }

    void siftDown(size_t i) {
        const size_t n = a_.size();
        while (true) {
            size_t left = 2 * i + 1, right = 2 * i + 2, best = i;
            if (left  < n && hasHigherPriority(a_[left],  a_[best])) best = left;
            if (right < n && hasHigherPriority(a_[right], a_[best])) best = right;
            if (best == i) break;
            swap(a_[i],a_[best]);
            i = best;
        }
    }
};
