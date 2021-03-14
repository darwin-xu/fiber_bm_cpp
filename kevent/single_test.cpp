#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    Kq<FdObj> kqMaster;
    Kq<FdObj> kqWorker;

    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes2(workers_num, requests_num);

    for (auto i = 0; i < workers_num; ++i)
    {
        kqMaster.regRead(master_read[i]);
        kqMaster.regWrite(master_write[i]);
        kqWorker.regRead(worker_read[i]);
        kqWorker.regWrite(worker_write[i]);
    }

    std::thread master([&kqMaster, &mwt = master_write] {
        while (true)
        {
            if (std::find_if(mwt.begin(), mwt.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == mwt.end())
                break;

            for (auto fdo : kqMaster.wait())
            {
                if (fdo->isRead())
                {
                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                }
                else
                {
                    operate(fdo->getFd(), QUERY_TEXT, write);
                    if (--fdo->getCount() == 0)
                    {
                        kqMaster.unreg(*fdo);
                    }
                }
            }
        }
    });

    // Main thread as the worker
    while (true)
    {
        if (std::find_if(worker_read.begin(),
                         worker_read.end(),
                         [](FdObj& fdo) -> bool {
                             return fdo.getCount() != 0;
                         }) == worker_read.end())
            break;

        auto fdos = kqWorker.wait();
        for (auto fdo : fdos)
        {
            if (fdo->isRead())
            {
                operate(fdo->getFd(), QUERY_TEXT, read);
                if (--fdo->getCount() == 0)
                {
                    kqWorker.unreg(*fdo);
                }
            }
            else
            {
                operate(fdo->getFd(), RESPONSE_TEXT, write);
            }
        }
    }

    master.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
