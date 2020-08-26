#include "../facility.hpp"
#include "../separated.hpp"

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

    std::thread wk([workers_num, requests_num, &worker_read, &worker_write]() {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(worker_read, worker_write);
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

    std::thread mt([workers_num, requests_num, &master_read, &master_write]() {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(master_read, master_write);

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
