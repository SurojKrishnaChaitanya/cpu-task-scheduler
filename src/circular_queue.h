#pragma once
#include <vector>
#include "process.h"
using namespace std;

class CircularQueue {
public:
    explicit CircularQueue(int capacity)
        : items_(capacity > 0 ? capacity : 1, nullptr),
          capacity_(capacity > 0 ? capacity : 1) {}

    bool empty() const { return count_ == 0; }
    bool full()  const { return count_ == capacity_; }
    int  size()  const { return count_; }

    bool enqueue(Process* p) {
        if (full()) return false;
        items_[rear_] = p;
        rear_ = (rear_ + 1) % capacity_;
        ++count_;
        return true;
    }

    Process* dequeue() {
        if (empty()) return nullptr;
        Process* p = items_[front_];
        items_[front_] = nullptr;
        front_ = (front_ + 1) % capacity_;
        --count_;
        return p;
    }

    vector<int> pids() const {
        vector<int> out;
        for (int i = 0; i < count_; ++i) out.push_back(items_[(front_ + i) % capacity_]->pid);
        return out;
    }

    Process* peek() const { return empty() ? nullptr : items_[front_]; }

private:
    vector<Process*> items_;
    int capacity_;
    int front_ = 0;
    int rear_  = 0;
    int count_ = 0;
};