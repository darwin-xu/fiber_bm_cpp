#ifndef FACILITY_H
#define FACILITY_H

#include <vector>
#include <thread>
#include <string>
#include <chrono>
#include <map>
#include <memory>
#include <algorithm>
#include <iostream>
#include <tuple>

#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include "FdObj.hpp"

using IntVector    = std::vector<int>;
using ThreadVector = std::vector<std::thread>;
using PIDVector    = std::vector<pid_t>;
using Int2IntMap   = std::map<int, int>;

extern std::string QUERY_TEXT;
extern std::string RESPONSE_TEXT;

template<class Function>
void readOrWrite(int fd, std::string& str, Function&& f)
{
    std::unique_ptr<char[]> temp = std::make_unique<char[]>(str.length());
    auto                    buf  = temp.get();

    memcpy(buf, str.c_str(), str.length());
    size_t remain = str.length();
    do
    {
        int r = f(fd, buf, remain);
        assert(r >= 0);
        buf += r;
        remain -= r;
    } while (remain != 0);
}

std::pair<IntVector, IntVector> sselect(const IntVector& rds, const IntVector& wts);

using TP = decltype(std::chrono::steady_clock::now());

void printStat(const TP& start, const TP& end, double workload);

std::tuple<IntVector, IntVector, IntVector, IntVector> initPipes1(int workers_number);

std::tuple<FdVector, FdVector, FdVector, FdVector> initPipes2(int workers_number, int requests_number);

#endif // FACILITY_H
