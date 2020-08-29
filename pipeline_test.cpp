#include <unistd.h>

#include "facility.hpp"

int main(int argc, char* argv[])
{
    auto requests_num = std::stoi(argv[1]);

    int  fildes1[2];
    auto r1 = pipe(fildes1);
    assert(r1 == 0);
    int  fildes2[2];
    auto r2 = pipe(fildes2);
    assert(r2 == 0);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < requests_num; ++i)
    {
        readOrWrite(fildes1[1], QUERY_TEXT, write);
        readOrWrite(fildes1[0], QUERY_TEXT, read);
        readOrWrite(fildes2[1], RESPONSE_TEXT, write);
        readOrWrite(fildes2[0], RESPONSE_TEXT, read);
    }

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, requests_num);

    return 0;
}
