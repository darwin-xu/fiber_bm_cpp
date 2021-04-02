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
#include <type_traits>

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
using FiberVector  = std::vector<boost::fibers::fiber>;

extern std::string QUERY_TEXT;
extern std::string RESPONSE_TEXT;

template<class Action, class YieldAt = void (*)(void)>
bool operate(
    int          fd,
    std::string& str,
    Action&&     rw,
    YieldAt&&    yieldAt = [] {
        assert(false && "yield function should be supplied.");
    })
{
    std::unique_ptr<char[]> temp = std::make_unique<char[]>(str.length());
    auto                    buf  = temp.get();

    if (std::is_same<decltype(rw), decltype(write)>::value)
        memcpy(buf, str.c_str(), str.length());

    size_t remain = str.length();
    do
    {
        // C++17: if statement with initializer
        // https://en.cppreference.com/w/cpp/language/if
        if (int r = rw(fd, buf, remain); r > 0)
        {
            buf += r;
            remain -= r;
        }
        else if (r == -1)
        {
            if (errno == EAGAIN)
                yieldAt();
            else
                assert(false && "read() or write() function fails.");
        }
        else if (r == 0)
            return false;
    } while (remain != 0);

    if (std::is_same<decltype(rw), decltype(read)>::value &&
        memcmp(buf, str.c_str(), str.length()))
        assert(false && "Data corrupted in transmission.");

    return true;
}

std::pair<IntVector, IntVector> sselect(const IntVector& rds,
                                        const IntVector& wts);

using TP = decltype(std::chrono::steady_clock::now());
struct StrTP
{
    // Although initialization list doesn't need it.
    // The vector::emplace_back() still needs it.
    // This can go away in the future:
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0960r3.html
    StrTP(const std::string& name, const TP& end)
        : name(name)
        , end(end)
    {
    }

    std::string name;
    TP          end;
};

using STPVector = std::vector<StrTP>;

void printStat(double workload, const TP& start, const TP& end);

std::tuple<IntVector, IntVector, IntVector, IntVector> initPipes1(
    int  pipesNumber,
    bool workerNonblock = false,
    bool clientNonblock = false);

std::tuple<FdVector, FdVector, FdVector, FdVector> initPipes2(
    int  pipesNumber,
    int  requestsNumber,
    bool workerNonblock = false,
    bool clientNonblock = false);

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

inline void removeFD(IntVector& iv, int fd)
{
    iv.erase(std::remove(iv.begin(), iv.end(), fd), iv.end());
}

#endif // FACILITY_H
