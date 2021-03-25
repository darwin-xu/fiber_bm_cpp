#include <utility>
#include <iostream>
#include <iomanip>

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
    assert(s != -1 && "select() function fails.");

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

void printStat(double workload, const TP& start, const TP& end)
{
    std::locale our_local(std::cout.getloc(), new separated);
    std::cout.imbue(our_local);

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::cout << "Duration time          : " << duration << " ms" << std::endl;
    std::cout << "Transactions Per Second: " << std::fixed
              << std::setprecision(0) << workload * 1000 / duration
              << std::endl;
}

std::tuple<IntVector, IntVector, IntVector, IntVector> initPipes1(
    int  pipesNumber,
    bool nonblock)
{
    IntVector workerRead;
    IntVector workerWrite;
    IntVector clientRead;
    IntVector clientWrite;

    for (auto i = 0; i < pipesNumber; ++i)
    {
        int  p1[2], p2[2];
        auto r1 = pipe(p1);
        assert(r1 == 0 && "Maybe too many opened files.");
        auto r2 = pipe(p2);
        assert(r2 == 0 && "Maybe too many opened files.");

        if (nonblock)
        {
            setNonblock(p1[0]);
            setNonblock(p1[1]);
            setNonblock(p2[0]);
            setNonblock(p2[1]);
        }

        workerRead.push_back(p1[0]);
        workerWrite.push_back(p2[1]);
        clientRead.push_back(p2[0]);
        clientWrite.push_back(p1[1]);
    }

    return std::make_tuple(workerRead, workerWrite, clientRead, clientWrite);
}

std::tuple<FdVector, FdVector, FdVector, FdVector>
initPipes2(int pipesNumber, int requestsNumber, bool nonblock)
{
    FdVector workerRead;
    FdVector workerWrite;
    FdVector clientRead;
    FdVector clientWrite;

    for (auto i = 0; i < pipesNumber; ++i)
    {
        int  p1[2], p2[2];
        auto r1 = pipe(p1);
        assert(r1 == 0 && "Maybe too many opened files.");
        auto r2 = pipe(p2);
        assert(r2 == 0 && "Maybe too many opened files.");

        if (nonblock)
        {
            setNonblock(p1[0]);
            setNonblock(p1[1]);
            setNonblock(p2[0]);
            setNonblock(p2[1]);
        }

        workerRead.emplace_back(p1[0], requestsNumber, true);
        workerWrite.emplace_back(p2[1], requestsNumber, false);
        clientRead.emplace_back(p2[0], requestsNumber, true);
        clientWrite.emplace_back(p1[1], requestsNumber, false);
    }

    return std::make_tuple(std::move(workerRead),
                           std::move(workerWrite),
                           std::move(clientRead),
                           std::move(clientWrite));
}

void setNonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags != -1 && "fcntl() function fails.");
    int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(r != -1 && "fcntl() function fails.");
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
