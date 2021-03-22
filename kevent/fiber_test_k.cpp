#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber] =
        parseArg2(argc, argv, "<clients number> <requests number>");

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes2(clientsNumber, requestsNumber, true);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    Kq<FdObj>   kqWorker;
    Kq<FdObj>   kqClient;
    auto        workersCount = clientsNumber;
    FiberVector workerFibers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        kqClient.regRead(clientRead[i]);
        kqClient.regWrite(clientWrite[i]);

        workerFibers.emplace_back(
            [rn = requestsNumber, &workersCount, &kqWorker](FdObj& fdoRead,
                                                            FdObj& fdoWrite) {
                for (auto n = 0; n < rn; ++n)
                {
                    kqWorker.regRead(fdoRead);
                    fdoRead.yield();
                    kqWorker.unreg(fdoRead);
                    operate(fdoRead.getFd(), QUERY_TEXT, read);

                    kqWorker.regWrite(fdoWrite);
                    fdoWrite.yield();
                    kqWorker.unreg(fdoWrite);
                    operate(fdoWrite.getFd(), RESPONSE_TEXT, write);
                }
                --workersCount;
            },
            std::ref(workerRead[i]),
            std::ref(workerWrite[i]));
    }

    boost::fibers::fiber reactorFiber([&workersCount, &kqWorker] {
        while (workersCount != 0)
        {
            for (auto fdo : kqWorker.wait())
            {
                fdo->resume();
                boost::this_fiber::yield();
            }
        }
    });

    std::thread client([&kqClient] {
        while (true)
        {
            auto fdos = kqClient.wait();
            if (fdos.empty())
                break;
            for (auto fdo : fdos)
            {
                if (--fdo->getCount() == 0)
                    kqClient.unreg(*fdo);

                if (fdo->isRead())
                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                else
                    operate(fdo->getFd(), QUERY_TEXT, write);
            }
        }
    });

    for (auto& wf : workerFibers)
        wf.join();
    reactorFiber.join();
    client.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, clientsNumber * requestsNumber);

    return 0;
}
