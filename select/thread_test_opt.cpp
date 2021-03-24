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

    auto worker = [cn  = clientsNumber,
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
        return std::chrono::steady_clock::now();
    };
    auto wf = std::async(std::launch::async, worker);

    auto client = [cn  = clientsNumber,
                   rn  = requestsNumber,
                   bn  = batchesNumber,
                   crd = clientRead,
                   cwt = clientWrite] {
        Int2IntMap pendingRead;
        Int2IntMap pendingWrite;
        for (auto fd : crd)
            pendingRead[fd] = rn;
        for (auto fd : cwt)
            pendingWrite[fd] = rn;

        while (!pendingRead.empty())
        {
            auto [readable, writeable] = sselect(crd, cwt);

            for (auto fd : readable)
            {
                operate(fd, RESPONSE_TEXT, read);
                if (--pendingRead[fd] == 0)
                    pendingRead.erase(fd);
            }
            for (auto fd : writeable)
            {
                if (pendingWrite[fd] > 0)
                {
                    for (int i = 0; i < bn; ++i)
                    {
                        --pendingWrite[fd];
                        operate(fd, QUERY_TEXT, write);
                    }
                }
            }
        }
        return std::chrono::steady_clock::now();
    };
    auto cf = std::async(std::launch::async, client);

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber,
              start,
              {{"worker", wf.get()}, {"client", cf.get()}});

    return 0;
}
