#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber, batchesNumber] =
        parseArg3(argc,
                  argv,
                  "<clients number> <requests number> <batches number>");
    assert(requestsNumber % batchesNumber == 0 &&
           "requests number should be divisible by batches number");
    bool usePipe = getEnvUsePipe();
    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initFDs1(clientsNumber, usePipe);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    std::thread worker([cn  = clientsNumber,
                        rn  = requestsNumber,
                        wrd = workerRead,
                        wrt = workerWrite] {
        auto pendingItems = cn * rn;
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

    std::thread client([rn  = requestsNumber,
                        bn  = batchesNumber,
                        rds = clientRead,
                        wts = clientWrite]() mutable {
        Int2IntMap pendingWrite;
        for (auto fd : wts)
            pendingWrite[fd] = rn;

        while (!rds.empty())
        {
            auto [readable, writeable] = sselect(rds, wts);

            for (auto fd : readable)
            {
                if (!operate(fd, RESPONSE_TEXT, read))
                    removeFD(rds, fd);
            }
            for (auto fd : writeable)
            {
                for (int i = 0; i < bn; ++i)
                {
                    operate(fd, QUERY_TEXT, write);
                    if (--pendingWrite[fd] == 0)
                    {
                        removeFD(wts, fd);
                        close(fd);
                        break;
                    }
                }
            }
        }
    });

    worker.join();
    client.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber, start, end);

    return 0;
}
