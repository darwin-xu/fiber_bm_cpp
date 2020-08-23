#include <string>
#include <thread>
#include <vector>
#include <map>
#include <utility>
#include <memory>
#include <iostream>
#include <chrono>
#include <iomanip>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <string.h>

#include <sys/select.h>

#include "separated.hpp"

using IntVector    = std::vector<int>;
using ThreadVector = std::vector<std::thread>;

std::string QUERY_TEXT    = "STATUS";
std::string RESPONSE_TEXT = "OK";

bool done = false;

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

std::pair<IntVector, IntVector> sselect(const IntVector& rds, const IntVector& wts)
{
    fd_set rdSet;
    fd_set wtSet;

    FD_ZERO(&rdSet);
    FD_ZERO(&wtSet);

    auto setFD_SET = [](const IntVector& fds, fd_set& set) {
        for (auto fd : fds) { FD_SET(fd, &set); }
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

int main(int argc, char* argv[])
{
    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    IntVector worker_read;
    IntVector worker_write;
    IntVector master_read;
    IntVector master_write;

    for (int i = 0; i < workers_num; ++i)
    {
        int fildes[2];

        assert(pipe(fildes) == 0);
        worker_read.push_back(fildes[0]);
        master_write.push_back(fildes[1]);

        assert(pipe(fildes) == 0);
        master_read.push_back(fildes[0]);
        worker_write.push_back(fildes[1]);
    }

    ThreadVector workers;
    for (int i = 0; i < workers_num; ++i)
    {
        workers.emplace_back([i, requests_num, &worker_read, &worker_write]() {
            for (int n = 0; n < requests_num; ++n)
            {
                readOrWrite(worker_read[i], QUERY_TEXT, read);
                readOrWrite(worker_write[i], RESPONSE_TEXT, write);
            }
        });
    }

    auto start = std::chrono::steady_clock::now();

    std::thread mt([&master_read, &master_write]() {
        while (!master_read.empty() && !master_write.empty())
        {
            auto [readable, writeable] = sselect(master_read, master_write);

            if (done)
                return;

            for (auto fd : readable) { readOrWrite(fd, RESPONSE_TEXT, read); }
            for (auto fd : writeable) { readOrWrite(fd, QUERY_TEXT, write); }
        }
    });

    for (auto& w : workers) { w.join(); }

    done = true;

    mt.join();

    auto end        = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    auto workload   = (double)workers_num * requests_num;

    std::cout << "elapsed time(ms): " << elapsed_ms << std::endl;
    std::cout << "items per second: " << std::fixed << std::setprecision(0) << workload * 1000 / elapsed_ms
              << std::endl;

    return 0;
}
