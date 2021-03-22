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

    PIDVector pv;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        auto pid = fork();
        assert(pid >= 0 && "fork() function fails");
        if (pid == 0)
        {
            for (auto n = 0; n < requestsNumber; ++n)
            {
                operate(workerRead[i], QUERY_TEXT, read);
                operate(workerWrite[i], RESPONSE_TEXT, write);
            }
            exit(0);
        }
        else
        {
            pv.push_back(pid);
        }
    }

    Int2IntMap pending_write_msgs;
    for (auto i = 0; i < clientsNumber; ++i)
        pending_write_msgs[i] = requestsNumber;
    Int2IntMap pending_read_msgs = pending_write_msgs;

    std::thread mt([cn  = clientsNumber,
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

    for (auto& pid : pv)
        waitpid(pid, NULL, 0);
    mt.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, clientsNumber * requestsNumber);

    return 0;
}
