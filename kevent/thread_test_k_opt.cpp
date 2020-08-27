#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "FdObj.hpp"

using FdVector = std::vector<FdObj>;

int main(int argc, char* argv[])
{
    Kq<FdObj> kqMaster;

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    FdVector     worker_read;
    FdVector     worker_write;
    FdVector     master_read;
    FdVector     master_write;
    ThreadVector workers;

    // NOTE:
    // Don't ever try to use the items in the vector while it still increasing!
    for (int i = 0; i < workers_num; ++i)
    {
        int p1[2], p2[2];
        assert(pipe(p1) == 0);
        assert(pipe(p2) == 0);

        worker_read.emplace_back(p1[0], requests_num, true);
        master_write.emplace_back(p1[1], requests_num, false);
        master_read.emplace_back(p2[0], requests_num, true);
        worker_write.emplace_back(p2[1], requests_num, false);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < workers_num; ++i)
    {
        kqMaster.regRead(master_read[i]);
        kqMaster.regWrite(master_write[i]);

        workers.emplace_back(
            [requests_num](FdObj& fdoRead, FdObj& fdoWrite) {
                Kq<FdObj> kq;
                kq.regRead(fdoRead);
                kq.regWrite(fdoWrite);

                while (true)
                {
                    if (fdoRead.getCount() == 0)
                        break;
                    auto fdos = kq.wait();
                    for (auto fdo : fdos)
                    {
                        if (fdo->isRead())
                        {
                            readOrWrite(fdo->getFd(), QUERY_TEXT, read);
                            --fdo->getCount();
                        }
                        else
                        {
                            readOrWrite(fdo->getFd(), RESPONSE_TEXT, write);
                        }
                    }
                }
            },
            std::ref(worker_read[i]),
            std::ref(worker_write[i]));
    }

    std::thread master([&kqMaster, &master_write]() {
        while (true)
        {
            if (std::find_if(master_write.begin(), master_write.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == master_write.end())
                break;

            auto fdos = kqMaster.wait();
            for (auto fdo : fdos)
            {
                if (fdo->isRead())
                {
                    readOrWrite(fdo->getFd(), RESPONSE_TEXT, read);
                }
                else
                {
                    readOrWrite(fdo->getFd(), QUERY_TEXT, write);
                    if (--fdo->getCount() == 0)
                    {
                        kqMaster.unreg(*fdo);
                    }
                }
            }
        }
    });

    for (auto& w : workers)
        w.join();
    master.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
