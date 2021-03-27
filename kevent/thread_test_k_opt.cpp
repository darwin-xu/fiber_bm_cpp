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

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    Kq<FdObj>    kqClient;
    ThreadVector workers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        kqClient.reg(clientRead[i]);
        kqClient.reg(clientWrite[i]);

        workers.emplace_back(
            [](FdObj& fdoRead, FdObj& fdoWrite) {
                Kq<FdObj> kq;
                kq.reg(fdoRead);
                kq.reg(fdoWrite);

                while (true)
                {
                    if (fdoRead.getCount() == 0)
                        break;
                    for (auto fdo : kq.wait())
                    {
                        if (fdo->isRead())
                        {
                            operate(fdo->getFd(), QUERY_TEXT, read);
                            --fdo->getCount();
                        }
                        else
                        {
                            operate(fdo->getFd(), RESPONSE_TEXT, write);
                        }
                    }
                }
            },
            std::ref(workerRead[i]),
            std::ref(workerWrite[i]));
    }

    std::thread client([&kqClient, &mwt = clientWrite] {
        while (true)
        {
            if (std::find_if(mwt.begin(), mwt.end(), [](FdObj& fdo) -> bool {
                    return fdo.getCount() != 0;
                }) == mwt.end())
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

    for (auto& w : workers)
        w.join();
    client.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber, start, end);

    return 0;
}
