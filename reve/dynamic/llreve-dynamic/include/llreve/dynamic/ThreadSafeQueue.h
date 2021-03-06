/*
 * This file is part of
 *    llreve - Automatic regression verification for LLVM programs
 *
 * Copyright (C) 2016 Karlsruhe Institute of Technology
 *
 * The system is published under a BSD license.
 * See LICENSE (distributed with this file) for details.
 */

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T> class ThreadSafeQueue {
  private:
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;

  public:
    void push(T &&val) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(val);
        }
        cv.notify_one();
    }
    void push(const T& val) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(val);
        }
        cv.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> lock(m);
        while (q.empty()) {
            cv.wait(lock);
        }
        T r = q.front();
        q.pop();
        return r;
    }
};
