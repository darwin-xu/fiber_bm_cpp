#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes1(workers_num);

    std::thread wk([workers_num, requests_num, wrd = worker_read, wrt = worker_write]() {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(wrd, wrt);
            for (auto fd : readable)
            {
                readOrWrite(fd, QUERY_TEXT, read);
                --pendingItems;
            }
            for (auto fd : writeable)
                readOrWrite(fd, RESPONSE_TEXT, write);
        }
    });

    auto start = std::chrono::steady_clock::now();

    std::thread mt([workers_num, requests_num, mrd = master_read, mwt = master_write]() {
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

    wk.join();
    mt.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
