#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber, batchesNumber] =
        parseArg3(argc,
                  argv,
                  "<clients number> <requests number> <batches number>");

    assert(requestsNumber % batchesNumber == 0);

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes1(clientsNumber);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector workers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        workers.emplace_back(
            [rn = requestsNumber](int rd, int wt) {
                for (auto n = 0; n < rn; ++n)
                {
                    operate(rd, QUERY_TEXT, read);
                    operate(wt, RESPONSE_TEXT, write);
                }
            },
            workerRead[i],
            workerWrite[i]);
    }

    ThreadVector clients;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        clients.emplace_back(
            [rn = requestsNumber, bn = batchesNumber](int rd, int wt) {
                for (auto n = 0; n < rn / bn; ++n)
                {
                    for (int j = 0; j < bn; ++j)
                    {
                        operate(wt, QUERY_TEXT, write);
                        operate(rd, RESPONSE_TEXT, read);
                    }
                }
            },
            clientRead[i],
            clientWrite[i]);
    }

    for (auto& w : workers)
        w.join();
    for (auto& c : clients)
        c.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(clientsNumber * requestsNumber));

    return 0;
}
