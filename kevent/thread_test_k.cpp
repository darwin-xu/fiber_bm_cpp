#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes2(workers_num, requests_num);

    Kq<FdObj> kq;

    ThreadVector workers;
    for (auto i = 0; i < workers_num; ++i)
    {
        kq.regRead(master_read[i]);
        kq.regWrite(master_write[i]);

        workers.emplace_back(
            [requests_num](FdObj& fdoRead, FdObj& fdoWrite) {
                for (auto n = 0; n < requests_num; ++n)
                {
                    readOrWrite(fdoRead.getFd(), QUERY_TEXT, read);
                    readOrWrite(fdoWrite.getFd(), RESPONSE_TEXT, write);
                }
            },
            std::ref(worker_read[i]),
            std::ref(worker_write[i]));
    }

    std::thread master([&kq, &mwt = master_write] {
        while (true)
        {
            if (std::find_if(mwt.begin(), mwt.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == mwt.end())
                break;

            for (auto fdo : kq.wait())
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
                        kq.unreg(*fdo);
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
