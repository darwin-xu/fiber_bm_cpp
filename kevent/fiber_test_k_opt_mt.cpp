#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber, threadsNumber, batchesNumber] =
        parseArg4(argc,
                  argv,
                  "<clients number> <requests number> <threads number> "
                  "<batches number>");

    assert(requestsNumber % batchesNumber == 0);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector fiberThreads;
    for (auto t = 0; t < threadsNumber; ++t)
    {
        fiberThreads.emplace_back(
            [t, cn = clientsNumber, rn = requestsNumber, bn = batchesNumber] {
                auto [workerRead, workerWrite, clientRead, clientWrite] =
                    initPipes2(cn, rn, true);

                Kq<FdObj> kqWorker;
                Kq<FdObj> kqClient;

                auto workersCount = cn;

                FiberVector workerFibers;
                for (auto i = 0; i < cn; ++i)
                {
                    kqClient.regRead(clientRead[i]);
                    kqClient.regWrite(clientWrite[i]);

                    workerFibers.emplace_back(
                        [rn, &workersCount, &kqWorker](FdObj& fdoRead,
                                                       FdObj& fdoWrite) {
                            for (auto n = 0; n < rn; ++n)
                            {
                                operate(fdoRead.getFd(),
                                        QUERY_TEXT,
                                        read,
                                        [&kqWorker, &fdoRead] {
                                            kqWorker.regRead(fdoRead);
                                            fdoRead.yield();
                                            kqWorker.unreg(fdoRead);
                                        });

                                operate(fdoWrite.getFd(),
                                        RESPONSE_TEXT,
                                        write,
                                        [&kqWorker, &fdoWrite] {
                                            kqWorker.regWrite(fdoWrite);
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
                            for (auto i = 0; i < bn; ++i)
                            {
                                if (--fdo->getCount() == 0)
                                    kqClient.unreg(*fdo);

                                if (fdo->isRead())
                                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                                else
                                    operate(fdo->getFd(), QUERY_TEXT, write);
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
    printStat(start, end, clientsNumber * requestsNumber * threadsNumber);

    return 0;
}
