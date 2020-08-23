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
#include <sys/wait.h>

#include "separated.h"

using IntVector    = std::vector<int>;
using ThreadVector = std::vector<std::thread>;
using IntIntMap    = std::map<int, int>;
using PIDVector    = std::vector<pid_t>;

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

    IntIntMap writable_idx;
    IntIntMap readable_idx;

    for (int i = 0; i < workers_num; ++i)
    {
        int fildes[2];

        assert(pipe(fildes) == 0);
        worker_read.push_back(fildes[0]);
        master_write.push_back(fildes[1]);
        writable_idx[fildes[1]] = i;

        assert(pipe(fildes) == 0);
        master_read.push_back(fildes[0]);
        worker_write.push_back(fildes[1]);
        readable_idx[fildes[0]] = i;
    }

    PIDVector pv;
    for (int i = 0; i < workers_num; ++i)
    {
        auto pid = fork();
        assert(pid >= 0);
        if (pid == 0)
        {
            for (int n = 0; n < requests_num; ++n)
            {
                readOrWrite(worker_read[i], QUERY_TEXT, read);
                readOrWrite(worker_write[i], RESPONSE_TEXT, write);
            }
            exit(0);
        }
        else
        {
            pv.push_back(pid);
        }
    }

    IntIntMap pending_write_msgs;
    for (int i = 0; i < workers_num; ++i) { pending_write_msgs[i] = requests_num; }
    IntIntMap pending_read_msgs = pending_write_msgs;

    auto start = std::chrono::steady_clock::now();

    std::thread mt(
        [&master_read, &master_write, &writable_idx, &readable_idx, &pending_write_msgs, &pending_read_msgs]() {
            while (!master_read.empty() && !master_write.empty())
            {
                auto [readable, writeable] = sselect(master_read, master_write);

                if (done)
                    return;

                for (auto fd : readable)
                {
                    readOrWrite(fd, RESPONSE_TEXT, read);
                    if (--pending_read_msgs[readable_idx[fd]] == 0)
                        master_read.erase(std::remove(master_read.begin(), master_read.end(), fd), master_read.end());
                }
                for (auto fd : writeable)
                {
                    readOrWrite(fd, QUERY_TEXT, write);
                    if (--pending_write_msgs[writable_idx[fd]] == 0)
                        master_write.erase(std::remove(master_write.begin(), master_write.end(), fd),
                                           master_write.end());
                }
            }
        });

    for (auto& pid : pv) { waitpid(pid, NULL, 0); }

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
