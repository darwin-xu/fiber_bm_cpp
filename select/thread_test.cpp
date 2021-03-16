#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber] =
        parseArg2(argc, argv, "<clients number> <requests number>");

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes1(clientsNumber);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector workers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        // Using something like this will cause compile error:
        // workers.emplace_back([i, requests_num, &worker_read, &worker_write]
        // it is an issue of C++ std:
        // http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#2313
        // https://stackoverflow.com/questions/46114214/lambda-implicit-capture-fails-with-variable-declared-from-structured-binding
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

    // "Captureing with the initializer" is a workaround.
    std::thread ct([cn  = clientsNumber,
                    rn  = requestsNumber,
                    crd = clientRead,
                    cwt = clientWrite] {
        auto pendingItems = cn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(crd, cwt);

            for (auto fd : readable)
                operate(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                operate(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& w : workers)
        w.join();
    ct.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(clientsNumber * requestsNumber));

    return 0;
}
