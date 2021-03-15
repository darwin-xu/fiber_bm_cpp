#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes1(workers_num);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    PIDVector pv;
    for (auto i = 0; i < workers_num; ++i)
    {
        auto pid = fork();
        assert(pid >= 0);
        if (pid == 0)
        {
            for (auto n = 0; n < requests_num; ++n)
            {
                operate(worker_read[i], QUERY_TEXT, read);
                operate(worker_write[i], RESPONSE_TEXT, write);
            }
            exit(0);
        }
        else
        {
            pv.push_back(pid);
        }
    }

    Int2IntMap pending_write_msgs;
    for (auto i = 0; i < workers_num; ++i)
        pending_write_msgs[i] = requests_num;
    Int2IntMap pending_read_msgs = pending_write_msgs;

    std::thread mt([wn  = workers_num,
                    rn  = requests_num,
                    mrd = master_read,
                    mwt = master_write] {
        auto pendingItems = wn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(mrd, mwt);

            for (auto fd : readable)
                operate(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                operate(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& pid : pv)
        waitpid(pid, NULL, 0);
    mt.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
