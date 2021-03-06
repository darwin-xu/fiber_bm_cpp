#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber, threadsNumber, batchesNumber] =
        parseArg4(argc,
                  argv,
                  "<clients number> <requests number> <threads number> "
                  "<batches number>");
    bool usePipe = getEnvUsePipe();

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector fiberThreads;
    for (auto t = 0; t < threadsNumber; ++t)
    {
        fiberThreads.emplace_back([usePipe,
                                   t,
                                   cn = clientsNumber,
                                   rn = requestsNumber,
                                   bn = batchesNumber] {
            auto [workerRead, workerWrite, clientRead, clientWrite] =
                initFDs2(cn, rn, usePipe, true, true);

            Kq<FdObj> kqWorker;
            Kq<FdObj> kqClient;

            auto workersCount = cn;

            FiberVector workerFibers;
            for (auto i = 0; i < cn; ++i)
            {
                kqClient.reg(clientRead[i]);
                kqClient.reg(clientWrite[i]);

                workerFibers.emplace_back(
                    [rn, &workersCount, &kqWorker](FdObj& fdoRead,
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

            std::thread client([&kqClient, bn] {
                while (true)
                {
                    auto fdos = kqClient.wait();
                    if (fdos.empty())
                        break;
                    for (auto fdo : fdos)
                    {
                        for (int i = 0; i < bn; ++i)
                        {
                            bool o = false;
                            if (fdo->isRead())
                                o = operate(fdo->getFd(), RESPONSE_TEXT, read);
                            else
                                o = operate(fdo->getFd(), QUERY_TEXT, write);

                            if (!o)
                            {
                                std::cout << "read/write error" << std::endl;
                                break;
                            }

                            if (--fdo->getCount() == 0)
                            {
                                kqClient.unreg(*fdo);
                                break;
                            }
                        }
                    }
                }
            });

            for (auto& f : workerFibers)
                f.join();
            reactorFiber.join();
            client.join();

#ifndef NDEBUG
            std::locale our_local(std::cout.getloc(), new separated);
            std::cout.imbue(our_local);
            std::cout << "[" << std::setw(2) << t
                      << "]: kqWorker.wait = " << std::setw(9)
                      << kqWorker.getWaitCount()
                      << " kqClient.wait = " << std::setw(9)
                      << kqClient.getWaitCount() << std::endl;
#endif
        });
    }

    for (auto& t : fiberThreads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber * threadsNumber, start, end);

    return 0;
}
