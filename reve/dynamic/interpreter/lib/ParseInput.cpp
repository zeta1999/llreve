#include "ParseInput.h"

#include <fstream>

// Each line contains one set of input values
// The format of a single line is vars1|vars2|heap1|heap2| where vars1 and vars2
// are semicolon separated list of numbers in the order the functions receive
// them and heap1 and heap2 are semicolon separated lists of comma separated
// tuples representing the heap index and the heap value

using std::vector;
using std::string;

vector<WorkItem> parseInput(string fileName) {
    vector<WorkItem> result;
    string line;
    std::ifstream fileStream(fileName.c_str());
    int i = 0;
    while (std::getline(fileStream, line)) {
        // split at '|'
        vector<string> parts = split(line, '|');
        if (parts.size() == 0) {
            return result;
        }
        assert(parts.size() == 4);
        result.push_back(
            {{getVariables(parts.at(0)), getVariables(parts.at(1))},
             {getHeap(parts.at(2)), getHeap(parts.at(3))},
             true,
             i});
        ++i;
    }
    return result;
}

std::vector<mpz_class> getVariables(std::string line) {
    // split at ';'
    vector<string> parts = split(line, ';');
    vector<mpz_class> result;
    for (auto p : parts) {
        mpz_class val(p);
        result.push_back(val);
    }
    return result;
}

Heap getHeap(std::string line) {
    // split at ';'
    vector<string> parts = split(line, ';');
    Heap result;
    for (auto p : parts) {
        vector<string> pairParts = split(p, ',');
        assert(pairParts.size() == 2);
        mpz_class index(pairParts.at(0));
        mpz_class val(pairParts.at(1));
        Integer intVal;
        if (BoundedFlag) {
            intVal = Integer(makeBoundedInt(8, val.get_si()));
        } else {
            intVal = Integer(val);
        }
        result.insert({Integer(index).asPointer(), intVal});
    }
    return result;
}

std::vector<std::string> &split(const std::string &s, char delim,
                                std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}
