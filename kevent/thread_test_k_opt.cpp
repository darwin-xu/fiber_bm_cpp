#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    Kq<FdObj> kqMaster;

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes2(workers_num, requests_num);

    auto start = std::chrono::steady_clock::now();

    ThreadVector workers;
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

    std::thread master([&kqMaster, &mwt = master_write]() {
        while (true)
        {
            if (std::find_if(mwt.begin(), mwt.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == mwt.end())
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
