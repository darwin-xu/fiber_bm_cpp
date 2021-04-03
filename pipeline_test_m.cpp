#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [requestsNumber, batchesNumber] =
        parseArg2(argc, argv, "<requests number> <batches number>");
    assert(requestsNumber % batchesNumber == 0 &&
           "requests number should be divisible by batches number");
    bool usePipe = getEnvUsePipe();
    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initFDs1(1, usePipe, true, true);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    std::thread worker([rd = workerRead[0], wt = workerWrite[0]]() {
        while (true)
        {
            if (!operate(rd, QUERY_TEXT, read, [] {
                }))
                break;
            operate(wt, RESPONSE_TEXT, write, [] {
            });
        }

        return std::chrono::steady_clock::now();
    });

    std::thread client([rn = requestsNumber,
                        bn = batchesNumber,
                        rd = clientRead[0],
                        wt = clientWrite[0]] {
        for (auto i = 0; i < rn / bn; ++i)
        {
            for (auto b = 0; b < bn; ++b)
                operate(wt, QUERY_TEXT, write, [] {
                });
            for (auto b = 0; b < bn; ++b)
                operate(rd, RESPONSE_TEXT, read, [] {
                });
        }
        close(rd);
        close(wt);

        return std::chrono::steady_clock::now();
    });

    worker.join();
    client.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(requestsNumber, start, end);

    return 0;
}
