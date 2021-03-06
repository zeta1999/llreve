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

#include "Interpreter.h"
#include "MonoPair.h"
#include "ThreadSafeQueue.h"

#include "gmpxx.h"

#include "llvm/IR/Function.h"

// All combinations of values inside the bounds, upperbound included
class Range {
    mpz_class lowerBound;
    mpz_class upperBound;
    size_t n;

  public:
    Range(mpz_class lowerBound, mpz_class upperBound, size_t n)
        : lowerBound(lowerBound), upperBound(upperBound), n(n) {}
    class RangeIterator
        : std::iterator<std::forward_iterator_tag, std::vector<mpz_class>> {
        mpz_class lowerBound;
        mpz_class upperBound;
        std::vector<mpz_class> vals;

      public:
        RangeIterator(mpz_class lowerBound, mpz_class upperBound,
                      std::vector<mpz_class> vals)
            : lowerBound(lowerBound), upperBound(upperBound), vals(vals) {}
        RangeIterator &operator++();
        bool operator==(const RangeIterator &other) {
            return vals == other.vals;
        }
        bool operator!=(const RangeIterator &other) {
            return vals != other.vals;
        }
        std::vector<mpz_class> &operator*() { return vals; }
    };
    RangeIterator begin();
    RangeIterator end();
};

struct WorkItem {
    MonoPair<std::vector<mpz_class>> vals;
    MonoPair<mpz_class> heapBackgrounds;
    MonoPair<llreve::dynamic::Heap> heaps;
    bool heapSet;
    int counter;
};
