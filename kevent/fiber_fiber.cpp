#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientNumber, requestsNumber, threadsNumber, batchesNumber] =
        parseArg4(argc,
                  argv,
                  "<clients number> <requests number> <threads number> "
                  "<batches number>");

    assert(requestsNumber % batchesNumber == 0);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector workerFiberThreads;
    for (auto t = 0; t < threadsNumber; ++t)
    {
        workerFiberThreads.emplace_back(
            [t, cn = clientNumber, rn = requestsNumber, bn = batchesNumber] {
                auto [workerRead, workerWrite, clientRead, clientWrite] =
                    initPipes2(cn, rn, true);

                Kq<FdObj> kqWorker;

                auto readOrYeild = [](Kq<FdObj>& kq, FdObj& fdo) {
                    kq.regRead(fdo);
                    fdo.yield();
                    kq.unreg(fdo);
                };

                auto writeOrYeild = [](Kq<FdObj>& kq, FdObj& fdo) {
                    kq.regWrite(fdo);
                    fdo.yield();
                    kq.unreg(fdo);
                };

                auto reactor = [](int& cnt, Kq<FdObj>& kq) {
                    while (cnt != 0)
                    {
                        for (auto fdo : kq.wait())
                        {
                            fdo->resume();
                            boost::this_fiber::yield();
                        }
                    }
                };

                Kq<FdObj>   kqClient;
                std::thread client([cn,
                                    rn,
                                    bn,
                                    &crd = clientRead,
                                    &cwt = clientWrite,
                                    &readOrYeild,
                                    &writeOrYeild,
                                    &reactor,
                                    &kqClient] {
                    auto        clientCount = cn;
                    FiberVector clientFibers;
                    for (auto i = 0; i < cn; ++i)
                    {
                        clientFibers.emplace_back(
                            [rn,
                             bn,
                             &clientCount,
                             &kqClient,
                             &readOrYeild,
                             &writeOrYeild](FdObj& fdoRead, FdObj& fdoWrite) {
                                for (auto n = 0; n < rn / bn; ++n)
                                {
                                    for (auto j = 0; j < bn; ++j)
                                    {
                                        operate(fdoWrite.getFd(),
                                                QUERY_TEXT,
                                                write,
                                                std::bind(writeOrYeild,
                                                          std::ref(kqClient),
                                                          std::ref(fdoWrite)));
                                    }
                                    for (auto j = 0; j < bn; ++j)
                                    {
                                        operate(fdoRead.getFd(),
                                                RESPONSE_TEXT,
                                                read,
                                                std::bind(readOrYeild,
                                                          std::ref(kqClient),
                                                          std::ref(fdoRead)));
                                    }
                                }
                                --clientCount;
                            },
                            std::ref(crd[i]),
                            std::ref(cwt[i]));
                    }

                    boost::fibers::fiber reactorFiber(reactor,
                                                      std::ref(clientCount),
                                                      std::ref(kqClient));

                    for (auto& c : clientFibers)
                        c.join();
                    reactorFiber.join();
                });

                auto        workersCount = cn;
                FiberVector workerFibers;
                for (auto i = 0; i < cn; ++i)
                {
                    workerFibers.emplace_back(
                        [rn,
                         &workersCount,
                         &kqWorker,
                         &readOrYeild,
                         &writeOrYeild](FdObj& fdoRead, FdObj& fdoWrite) {
                            for (auto n = 0; n < rn; ++n)
                            {
                                operate(fdoRead.getFd(),
                                        QUERY_TEXT,
                                        read,
                                        std::bind(readOrYeild,
                                                  std::ref(kqWorker),
                                                  std::ref(fdoRead)));
                                operate(fdoWrite.getFd(),
                                        RESPONSE_TEXT,
                                        write,
                                        std::bind(writeOrYeild,
                                                  std::ref(kqWorker),
                                                  std::ref(fdoWrite)));
                            }
                            --workersCount;
                        },
                        std::ref(workerRead[i]),
                        std::ref(workerWrite[i]));
                }
                boost::fibers::fiber reactorFiber(reactor,
                                                  std::ref(workersCount),
                                                  std::ref(kqWorker));

                for (auto& w : workerFibers)
                    w.join();
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

    for (auto& t : workerFiberThreads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, clientNumber * requestsNumber * threadsNumber);

    return 0;
}
