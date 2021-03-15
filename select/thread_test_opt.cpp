#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num, batches_num] =
        parseArg3(argc,
                  argv,
                  "<workers number> <requests number> <batches number>");

    assert(requests_num % batches_num == 0);

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes1(workers_num);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    std::thread wk([wn  = workers_num,
                    rn  = requests_num,
                    wrd = worker_read,
                    wrt = worker_write] {
        auto pendingItems = wn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(wrd, wrt);
            for (auto fd : readable)
            {
                operate(fd, QUERY_TEXT, read);
                --pendingItems;
            }
            for (auto fd : writeable)
                operate(fd, RESPONSE_TEXT, write);
        }
    });

    std::thread mt([wn  = workers_num,
                    rn  = requests_num,
                    bn  = batches_num,
                    mrd = master_read,
                    mwt = master_write] {
        auto pendingItems = wn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(mrd, mwt);

            for (int i = 0; i < bn; ++i)
            {
                for (auto fd : readable)
                    operate(fd, RESPONSE_TEXT, read);
                for (auto fd : writeable)
                {
                    operate(fd, QUERY_TEXT, write);
                    --pendingItems;
                }
            }
        }
    });

    wk.join();
    mt.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
