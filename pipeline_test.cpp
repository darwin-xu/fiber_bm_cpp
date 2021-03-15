#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto requests_num = parseArg1(argc, argv, "<requests number>");

    int  fildes1[2];
    auto r1 = pipe(fildes1);
    assert(r1 == 0);
    int  fildes2[2];
    auto r2 = pipe(fildes2);
    assert(r2 == 0);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    for (auto i = 0; i < requests_num; ++i)
    {
        operate(fildes1[1], QUERY_TEXT, write);
        operate(fildes1[0], QUERY_TEXT, read);
        operate(fildes2[1], RESPONSE_TEXT, write);
        operate(fildes2[0], RESPONSE_TEXT, read);
    }

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, requests_num);

    return 0;
}
