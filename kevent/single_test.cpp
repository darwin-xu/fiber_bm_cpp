#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber] =
        parseArg2(argc, argv, "<clients number> <requests number>");

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes2(clientsNumber, requestsNumber);

    Kq<FdObj> kqClient;
    Kq<FdObj> kqWorker;

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    for (auto i = 0; i < clientsNumber; ++i)
    {
        kqClient.reg(clientRead[i]);
        kqClient.reg(clientWrite[i]);
        kqWorker.reg(workerRead[i]);
        kqWorker.reg(workerWrite[i]);
    }

    std::thread client([&kqClient, &cw = clientWrite] {
        while (true)
        {
            if (std::find_if(cw.begin(), cw.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == cw.end())
                break;

            for (auto fdo : kqClient.wait())
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
                        kqClient.unreg(*fdo);
                    }
                }
            }
        }
    });

    // Main thread as the worker
    while (true)
    {
        if (std::find_if(workerRead.begin(),
                         workerRead.end(),
                         [](FdObj& fdo) -> bool {
                             return fdo.getCount() != 0;
                         }) == workerRead.end())
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

    client.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber, start, end);

    return 0;
}
