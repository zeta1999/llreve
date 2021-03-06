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

template <typename T> bool isZero(const std::vector<T> &a) {
    for (auto &val : a) {
        if (val != 0) {
            return false;
        }
    }
    return true;
}

// The outer vector indicates the row
template <typename T> using Matrix = std::vector<std::vector<T>>;

void reducedRowEchelonForm(Matrix<mpq_class> &input);

template <typename T> void dumpMatrix(const Matrix<T> &m) {
    for (const auto &row : m) {
        for (const auto &col : row) {
            std::cout << col.get_str() << "\t";
        }
        std::cout << "\n";
    }
}

bool linearlyIndependent(const Matrix<mpq_class> &vectors,
                         std::vector<mpq_class> newVector);
std::vector<std::vector<mpq_class>> nullSpace(const Matrix<mpq_class> &m);
std::vector<mpq_class> multiplyRow(std::vector<mpq_class> vec, mpq_class c);
std::vector<mpz_class> ratToInt(std::vector<mpq_class> vec);
template <typename T>
std::vector<T> matrixTimesVector(const Matrix<T> &m,
                                 const std::vector<T> &vec) {
    std::vector<T> result(m.size(), 0);
    size_t j = 0;
    for (const auto &row : m) {
        assert(row.size() == vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            result.at(j) += row.at(i) * vec.at(i);
        }
        ++j;
    }
    return result;
}
