#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes2(workers_num, requests_num, true);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    Kq<FdObj>   kqWorker;
    Kq<FdObj>   kqMaster;
    auto        workers_cnt = workers_num;
    FiberVector workerFibers;
    for (auto i = 0; i < workers_num; ++i)
    {
        kqMaster.regRead(master_read[i]);
        kqMaster.regWrite(master_write[i]);

        workerFibers.emplace_back(
            [rn = requests_num, &workers_cnt, &kqWorker](FdObj& fdoRead,
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

    std::thread master([&kqMaster] {
        while (true)
        {
            auto fdos = kqMaster.wait();
            if (fdos.empty())
                break;
            for (auto fdo : fdos)
            {
                if (--fdo->getCount() == 0)
                    kqMaster.unreg(*fdo);

                if (fdo->isRead())
                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                else
                    operate(fdo->getFd(), QUERY_TEXT, write);
            }
        }
    });

    for (auto& f : workerFibers)
        f.join();
    reactorFiber.join();
    master.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
