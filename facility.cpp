#include <utility>
#include <iostream>

#include <unistd.h>

#include "facility.hpp"
#include "separated.hpp"

std::string QUERY_TEXT    = "STATUS";
std::string RESPONSE_TEXT = "OK";

std::pair<IntVector, IntVector> sselect(const IntVector& rds, const IntVector& wts)
{
    fd_set rdSet;
    fd_set wtSet;

    FD_ZERO(&rdSet);
    FD_ZERO(&wtSet);

    auto setFD_SET = [](const IntVector& fds, fd_set& set) {
        for (auto fd : fds)
            FD_SET(fd, &set);
    };

    setFD_SET(rds, rdSet);
    setFD_SET(wts, wtSet);

    struct timeval timeout;
    timeout.tv_sec  = 10;
    timeout.tv_usec = 1000;

    assert(select(FD_SETSIZE, &rdSet, &wtSet, NULL, &timeout) != -1);

    auto getFD_SET = [](const IntVector& fds, const fd_set& set) -> IntVector {
        IntVector retSet;
        for (auto fd : fds)
        {
            if (FD_ISSET(fd, &set))
                retSet.push_back(fd);
        }
        return retSet;
    };

    return std::make_pair(getFD_SET(rds, rdSet), getFD_SET(wts, wtSet));
}

template<class T>
T kkevent()
{
    T t;
    return t;
}

void printStat(const TP& start, const TP& end, double workload)
{
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::locale our_local(std::cout.getloc(), new separated);
    std::cout.imbue(our_local);
    std::cout << "elapsed time(ms): " << elapsed_ms << std::endl;
    std::cout << "items per second: " << std::fixed << std::setprecision(0) << workload * 1000 / elapsed_ms
              << std::endl;
}
