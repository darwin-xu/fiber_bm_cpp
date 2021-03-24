#ifndef FACILITY_H
#define FACILITY_H

#include <vector>
#include <thread>
#include <string>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <iostream>
#include <tuple>
#include <future>

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
using Int2IntMap   = std::unordered_map<int, int>;

extern std::string QUERY_TEXT;
extern std::string RESPONSE_TEXT;

template<class Action, class YieldAt = void (*)(void)>
void operate(
    int          fd,
    std::string& str,
    Action&&     rw,
    YieldAt&&    yieldAt = [] {
    })
{
    std::unique_ptr<char[]> temp = std::make_unique<char[]>(str.length());
    auto                    buf  = temp.get();
    memcpy(buf, str.c_str(), str.length());
    size_t remain = str.length();
    do
    {
        if (int r = rw(fd, buf, remain); r != -1)
        {
            buf += r;
            remain -= r;
        }
        else
        {
            if (errno == EAGAIN)
                yieldAt();
            else
                assert(false && "read() or write() function fails.");
        }
    } while (remain != 0);
}

std::pair<IntVector, IntVector> sselect(const IntVector& rds,
                                        const IntVector& wts);

using TP = decltype(std::chrono::steady_clock::now());

using TPVector = std::vector<TP>;

void printStat(double workload, const TP& start, const TPVector& end);

std::tuple<IntVector, IntVector, IntVector, IntVector> initPipes1(
    int  pipesNumber,
    bool nonblock = false);

std::tuple<FdVector, FdVector, FdVector, FdVector>
initPipes2(int pipesNumber, int requestsNumber, bool nonblock = false);

void setNonblock(int fd);

#define TF \
    std::this_thread::get_id() << "_" << boost::this_fiber::get_id() << ":"

int parseArg1(int argc, char* argv[], const std::string& prompt);

std::tuple<int, int> parseArg2(int                argc,
                               char*              argv[],
                               const std::string& prompt);

std::tuple<int, int, int> parseArg3(int                argc,
                                    char*              argv[],
                                    const std::string& prompt);

std::tuple<int, int, int, int> parseArg4(int                argc,
                                         char*              argv[],
                                         const std::string& prompt);

#endif // FACILITY_H
