#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto requestsNumber = parseArg1(argc, argv, "<requests number>");
    bool usePipe        = getEnvUsePipe();
    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initFDs1(1, usePipe);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    for (auto i = 0; i < requestsNumber; ++i)
    {
        operate(clientWrite[0], QUERY_TEXT, write);
        operate(workerRead[0], QUERY_TEXT, read);
        operate(workerWrite[0], RESPONSE_TEXT, write);
        operate(clientRead[0], RESPONSE_TEXT, read);
    }

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(requestsNumber, start, end);

    return 0;
}
