#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);
    auto threads_num  = std::stoi(argv[3]);
    auto batch_num    = std::stoi(argv[4]);

    ThreadVector fiberThreads;
    for (auto t = 0; t < threads_num; ++t)
    {
        fiberThreads.emplace_back([t, workers_num, requests_num, batch_num] {
            auto [worker_read, worker_write, master_read, master_write] = initPipes2(workers_num, requests_num, true);

            Kq<FdObj> kqWorker;
            Kq<FdObj> kqMaster;

            auto workers_cnt = workers_num;

            FiberVector workerFibers;
            for (auto i = 0; i < workers_num; ++i)
            {
                kqMaster.regRead(master_read[i]);
                kqMaster.regWrite(master_write[i]);

                workerFibers.emplace_back(
                    [requests_num, &workers_cnt, &kqWorker](FdObj& fdoRead, FdObj& fdoWrite) {
                        for (auto n = 0; n < requests_num; ++n)
                        {
                            readOrWrite(fdoRead.getFd(), QUERY_TEXT, read, [&kqWorker, &fdoRead] {
                                kqWorker.regRead(fdoRead);
                                fdoRead.yield();
                                kqWorker.unreg(fdoRead);
                            });

                            readOrWrite(fdoWrite.getFd(), RESPONSE_TEXT, write, [&kqWorker, &fdoWrite] {
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

            std::thread master([&kqMaster, batch_num] {
                while (true)
                {
                    auto fdos = kqMaster.wait();
                    if (fdos.empty())
                        break;
                    for (auto fdo : fdos)
                    {
                        for (auto i = 0; i < batch_num; ++i)
                        {
                            if (--fdo->getCount() == 0)
                                kqMaster.unreg(*fdo);

                            if (fdo->isRead())
                                readOrWrite(fdo->getFd(), RESPONSE_TEXT, read);
                            else
                                readOrWrite(fdo->getFd(), QUERY_TEXT, write);
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
            std::cout << "[" << std::setw(2) << t << "]: kqWorker.wait = " << std::setw(9) << kqWorker.getWaitCount()
                      << " kqMaster.wait = " << std::setw(9) << kqMaster.getWaitCount() << std::endl;
#endif
        });
    }

    for (auto& t : fiberThreads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num * threads_num));

    return 0;
}
