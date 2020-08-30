#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes1(workers_num);

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

    Int2IntMap pending_write_msgs;
    for (int i = 0; i < workers_num; ++i)
        pending_write_msgs[i] = requests_num;
    Int2IntMap pending_read_msgs = pending_write_msgs;

    std::thread mt([workers_num, requests_num, mrd = master_read, mwt = master_write] {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(mrd, mwt);

            for (auto fd : readable)
                readOrWrite(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                readOrWrite(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& pid : pv)
        waitpid(pid, NULL, 0);
    mt.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
