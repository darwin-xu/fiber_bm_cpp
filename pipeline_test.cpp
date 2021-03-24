#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto requestsNumber = parseArg1(argc, argv, "<requests number>");

    int  fildes1[2];
    auto r1 = pipe(fildes1);
    assert(r1 == 0 && "Maybe too many opened files.");
    int  fildes2[2];
    auto r2 = pipe(fildes2);
    assert(r2 == 0 && "Maybe too many opened files.");

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    for (auto i = 0; i < requestsNumber; ++i)
    {
        operate(fildes1[1], QUERY_TEXT, write);    // Client send request
        operate(fildes1[0], QUERY_TEXT, read);     // Server receive request
        operate(fildes2[1], RESPONSE_TEXT, write); // Server send response
        operate(fildes2[0], RESPONSE_TEXT, read);  // Client receive response
    }

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(requestsNumber, start, {end});

    return 0;
}
