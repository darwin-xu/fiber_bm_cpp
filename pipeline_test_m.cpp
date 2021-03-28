#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [requestsNumber, batchesNumber] =
        parseArg2(argc, argv, "<requests number> <batches number>");

    assert(requestsNumber % batchesNumber == 0 &&
           "requests number should be divisible by batches number");

    int  fildes1[2];
    auto r1 = pipe(fildes1);
    assert(r1 == 0 && "Maybe too many opened files.");
    int  fildes2[2];
    auto r2 = pipe(fildes2);
    assert(r2 == 0 && "Maybe too many opened files.");

    setNonblock(fildes1[0]);
    setNonblock(fildes1[1]);
    setNonblock(fildes2[0]);
    setNonblock(fildes2[1]);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    std::thread worker([rd = fildes2[0], wt = fildes1[1]]() {
        while (true)
        {
            if (!operate(rd, QUERY_TEXT, read))
                break;
            operate(wt, RESPONSE_TEXT, write);
        }

        return std::chrono::steady_clock::now();
    });

    std::thread client([rn = requestsNumber,
                        bn = batchesNumber,
                        rd = fildes1[0],
                        wt = fildes2[1]] {
        for (auto i = 0; i < rn / bn; ++i)
        {
            for (auto b = 0; b < bn; ++b)
                operate(wt, QUERY_TEXT, write);
            for (auto b = 0; b < bn; ++b)
                operate(rd, RESPONSE_TEXT, read);
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