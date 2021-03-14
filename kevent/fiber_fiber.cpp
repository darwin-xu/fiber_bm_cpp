#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto [workers_num, requests_num, threads_num, batches_num] = parseArg4(
        argc,
        argv,
        "<workers number> <requests number> <threads number> <batches number>");

    assert(requests_num % batches_num == 0);

    ThreadVector workerFiberThreads;
    for (auto t = 0; t < threads_num; ++t)
    {
        workerFiberThreads.emplace_back(
            [t, wn = workers_num, rn = requests_num, bn = batches_num] {
                auto [worker_read, worker_write, master_read, master_write] =
                    initPipes2(wn, rn, true);

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

                Kq<FdObj>   kqMaster;
                std::thread master([wn,
                                    rn,
                                    bn,
                                    &mrd = master_read,
                                    &mwt = master_write,
                                    &readOrYeild,
                                    &writeOrYeild,
                                    &reactor,
                                    &kqMaster] {
                    auto        masters_cnt = wn;
                    FiberVector masterFibers;
                    for (auto i = 0; i < wn; ++i)
                    {
                        masterFibers.emplace_back(
                            [rn,
                             bn,
                             &masters_cnt,
                             &kqMaster,
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
                                                          std::ref(kqMaster),
                                                          std::ref(fdoWrite)));
                                    }
                                    for (auto j = 0; j < bn; ++j)
                                    {
                                        operate(fdoRead.getFd(),
                                                RESPONSE_TEXT,
                                                read,
                                                std::bind(readOrYeild,
                                                          std::ref(kqMaster),
                                                          std::ref(fdoRead)));
                                    }
                                }
                                --masters_cnt;
                            },
                            std::ref(mrd[i]),
                            std::ref(mwt[i]));
                    }

                    boost::fibers::fiber reactorFiber(reactor,
                                                      std::ref(masters_cnt),
                                                      std::ref(kqMaster));

                    for (auto& m : masterFibers)
                        m.join();
                    reactorFiber.join();
                });

                auto        workers_cnt = wn;
                FiberVector workerFibers;
                for (auto i = 0; i < wn; ++i)
                {
                    workerFibers.emplace_back(
                        [rn,
                         &workers_cnt,
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
                            --workers_cnt;
                        },
                        std::ref(worker_read[i]),
                        std::ref(worker_write[i]));
                }
                boost::fibers::fiber reactorFiber(reactor,
                                                  std::ref(workers_cnt),
                                                  std::ref(kqWorker));

                for (auto& w : workerFibers)
                    w.join();
                reactorFiber.join();
                master.join();

#ifndef NDEBUG
                std::locale our_local(std::cout.getloc(), new separated);
                std::cout.imbue(our_local);
                std::cout << "[" << std::setw(2) << t
                          << "]: kqWorker.wait = " << std::setw(9)
                          << kqWorker.getWaitCount()
                          << " kqMaster.wait = " << std::setw(9)
                          << kqMaster.getWaitCount() << std::endl;
#endif
            });
    }

    for (auto& t : workerFiberThreads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start,
              end,
              static_cast<double>(workers_num * requests_num * threads_num));

    return 0;
}
