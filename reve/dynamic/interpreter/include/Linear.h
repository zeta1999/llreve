#pragma once

#include "Interpreter.h"

bool linearlyIndependent(const std::vector<VarIntVal> &a,
                         const std::vector<VarIntVal> &b);
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

Matrix<mpq_class> rowEchelonForm(Matrix<mpq_class> input);
size_t rank(const Matrix<mpq_class> &m);

template <typename T> void dumpMatrix(const Matrix<T> &m) {
    for (const auto &row : m) {
        for (const auto &col : row) {
            std::cout << col.get_str() << "\t";
        }
        std::cout << "\n";
    }
}

bool linearlyIndependent(std::vector<std::vector<mpq_class>> vectors);
std::vector<std::vector<mpq_class>> nullSpace(const Matrix<mpq_class> &m);
std::vector<mpq_class> multiplyRow(std::vector<mpq_class> vec, mpq_class c);
std::vector<mpz_class> ratToInt(std::vector<mpq_class> vec);
