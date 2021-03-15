#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes2(workers_num, requests_num);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    Kq<FdObj>    kq;
    ThreadVector workers;
    for (auto i = 0; i < workers_num; ++i)
    {
        kq.regRead(master_read[i]);
        kq.regWrite(master_write[i]);

        workers.emplace_back(
            [rn = requests_num](FdObj& fdoRead, FdObj& fdoWrite) {
                for (auto n = 0; n < rn; ++n)
                {
                    operate(fdoRead.getFd(), QUERY_TEXT, read);
                    operate(fdoWrite.getFd(), RESPONSE_TEXT, write);
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
                    operate(fdo->getFd(), RESPONSE_TEXT, read);
                }
                else
                {
                    operate(fdo->getFd(), QUERY_TEXT, write);
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

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
