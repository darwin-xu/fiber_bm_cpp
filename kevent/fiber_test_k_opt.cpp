#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber, threadsNumber] =
        parseArg3(argc,
                  argv,
                  "<clients number> <requests number> <threads number>");

    bool usePipe = getEnvUsePipe();
    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initFDs2(clientsNumber, requestsNumber, usePipe, true, false);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    // boost::fibers::use_scheduling_algorithm<FifoScheduler>();

    Kq<FdObj>              kqWorker;
    std::vector<Kq<FdObj>> kqClients(threadsNumber);
    auto                   workersCount = clientsNumber;
    FiberVector            workerFibers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        kqClients[i % threadsNumber].reg(clientRead[i]);
        kqClients[i % threadsNumber].reg(clientWrite[i]);

        workerFibers.emplace_back(
            [rn = requestsNumber, &workersCount, &kqWorker](FdObj& fdoRead,
                                                            FdObj& fdoWrite) {
                for (auto n = 0; n < rn; ++n)
                {
                    operate(fdoRead.getFd(),
                            QUERY_TEXT,
                            read,
                            [&kqWorker, &fdoRead] {
                                kqWorker.reg(fdoRead);
                                fdoRead.yield();
                                kqWorker.unreg(fdoRead);
                            });

                    operate(fdoWrite.getFd(),
                            RESPONSE_TEXT,
                            write,
                            [&kqWorker, &fdoWrite] {
                                kqWorker.reg(fdoWrite);
                                fdoWrite.yield();
                                kqWorker.unreg(fdoWrite);
                            });
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

    auto client = [](Kq<FdObj> kq) {
        while (true)
        {
            auto fdos = kq.wait();
            if (fdos.empty())
                break;
            for (auto fdo : fdos)
            {
                if (fdo->isRead())
                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                else
                    operate(fdo->getFd(), QUERY_TEXT, write);

                if (--fdo->getCount() == 0)
                {
                    kq.unreg(*fdo);
                    break;
                }
            }
        }
    };

    ThreadVector clients;
    for (auto i = 0; i < threadsNumber; ++i)
        clients.emplace_back(client, std::ref(kqClients[i]));

    for (auto& f : workerFibers)
        f.join();
    reactorFiber.join();

    for (auto& c : clients)
        c.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber, start, end);

    return 0;
}
