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

    ThreadVector fiberThreads;
    for (auto t = 0; t < threads_num; ++t)
    {
        fiberThreads.emplace_back(
            [t, wn = workers_num, rn = requests_num, bn = batches_num] {
                auto [worker_read, worker_write, master_read, master_write] =
                    initPipes2(wn, rn, true);

                Kq<FdObj> kqWorker;
                Kq<FdObj> kqMaster;

                auto workers_cnt = wn;

                FiberVector workerFibers;
                for (auto i = 0; i < wn; ++i)
                {
                    kqMaster.regRead(master_read[i]);
                    kqMaster.regWrite(master_write[i]);

                    workerFibers.emplace_back(
                        [rn, &workers_cnt, &kqWorker](FdObj& fdoRead,
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
                            --workers_cnt;
                        },
                        std::ref(worker_read[i]),
                        std::ref(worker_write[i]));
                }

                boost::fibers::fiber reactorFiber([&workers_cnt, &kqWorker] {
                    while (workers_cnt != 0)
                    {
                        for (auto fdo : kqWorker.wait())
                        {
                            fdo->resume();
                            boost::this_fiber::yield();
                        }
                    }
                });

                std::thread master([&kqMaster, bn] {
                    while (true)
                    {
                        auto fdos = kqMaster.wait();
                        if (fdos.empty())
                            break;
                        for (auto fdo : fdos)
                        {
                            for (auto i = 0; i < bn; ++i)
                            {
                                if (--fdo->getCount() == 0)
                                    kqMaster.unreg(*fdo);

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

    for (auto& t : fiberThreads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start,
              end,
              static_cast<double>(workers_num * requests_num * threads_num));

    return 0;
}
