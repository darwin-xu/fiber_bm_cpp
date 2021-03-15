#include <utility>
#include <iostream>

#include <unistd.h>

#include "facility.hpp"
#include "separated.hpp"

std::string QUERY_TEXT    = "STATUS";
std::string RESPONSE_TEXT = "OK";

std::pair<IntVector, IntVector> sselect(const IntVector& rds,
                                        const IntVector& wts)
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

    auto s = select(FD_SETSIZE, &rdSet, &wtSet, NULL, &timeout);
    assert(s != -1);

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
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::locale our_local(std::cout.getloc(), new separated);
    std::cout.imbue(our_local);
    std::cout << "Elapsed time(ms)       : " << elapsed_ms << std::endl;
    std::cout << "Transactions per second: " << std::fixed
              << std::setprecision(0) << workload * 1000 / elapsed_ms
              << std::endl;
}

std::tuple<IntVector, IntVector, IntVector, IntVector> initPipes1(
    int  workers_number,
    bool nonblock)
{
    IntVector worker_read;
    IntVector worker_write;
    IntVector client_read;
    IntVector client_write;

    for (auto i = 0; i < workers_number; ++i)
    {
        int  p1[2], p2[2];
        auto r1 = pipe(p1);
        assert(r1 == 0);
        auto r2 = pipe(p2);
        assert(r2 == 0);

        if (nonblock)
        {
            setNonblock(p1[0]);
            setNonblock(p1[1]);
            setNonblock(p2[0]);
            setNonblock(p2[1]);
        }

        worker_read.push_back(p1[0]);
        client_write.push_back(p1[1]);
        client_read.push_back(p2[0]);
        worker_write.push_back(p2[1]);
    }

    return std::make_tuple(worker_read,
                           worker_write,
                           client_read,
                           client_write);
}

std::tuple<FdVector, FdVector, FdVector, FdVector>
initPipes2(int workers_number, int requests_number, bool nonblock)
{
    FdVector worker_read;
    FdVector worker_write;
    FdVector client_read;
    FdVector client_write;

    for (auto i = 0; i < workers_number; ++i)
    {
        int  p1[2], p2[2];
        auto r1 = pipe(p1);
        assert(r1 == 0);
        auto r2 = pipe(p2);
        assert(r2 == 0);

        if (nonblock)
        {
            setNonblock(p1[0]);
            setNonblock(p1[1]);
            setNonblock(p2[0]);
            setNonblock(p2[1]);
        }

        worker_read.emplace_back(p1[0], requests_number, true);
        client_write.emplace_back(p1[1], requests_number, false);
        client_read.emplace_back(p2[0], requests_number, true);
        worker_write.emplace_back(p2[1], requests_number, false);
    }

    return std::make_tuple(std::move(worker_read),
                           std::move(worker_write),
                           std::move(client_read),
                           std::move(client_write));
}

void setNonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags != -1);
    int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(r != -1);
}

auto checkArgCount =
    [](int needArgc, int argc, char* argv[], const std::string& prompt) {
        if (argc < needArgc)
        {
            std::cout << argv[0] << " " << prompt << std::endl;
            exit(-1);
        }
    };

int parseArg1(int argc, char* argv[], const std::string& prompt)
{
    checkArgCount(2, argc, argv, prompt);
    return std::stoi(argv[1]);
}

std::tuple<int, int> parseArg2(int                argc,
                               char*              argv[],
                               const std::string& prompt)
{
    checkArgCount(3, argc, argv, prompt);
    return std::make_tuple(std::stoi(argv[1]), std::stoi(argv[2]));
}

std::tuple<int, int, int> parseArg3(int                argc,
                                    char*              argv[],
                                    const std::string& prompt)
{
    checkArgCount(4, argc, argv, prompt);
    return std::make_tuple(std::stoi(argv[1]),
                           std::stoi(argv[2]),
                           std::stoi(argv[3]));
}

std::tuple<int, int, int, int> parseArg4(int                argc,
                                         char*              argv[],
                                         const std::string& prompt)
{
    checkArgCount(5, argc, argv, prompt);
    return std::make_tuple(std::stoi(argv[1]),
                           std::stoi(argv[2]),
                           std::stoi(argv[3]),
                           std::stoi(argv[4]));
}
