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

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes1(clientsNumber);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    std::thread wk([cn  = clientsNumber,
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

    std::thread ct([cn  = clientsNumber,
                    rn  = requestsNumber,
                    bn  = batchesNumber,
                    crd = clientRead,
                    cwt = clientWrite] {
        auto pendingItems = cn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(crd, cwt);

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
    ct.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, clientsNumber * requestsNumber);

    return 0;
}
