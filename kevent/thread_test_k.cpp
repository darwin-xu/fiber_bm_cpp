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

    Kq<FdObj>    kq;
    ThreadVector workers;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        kq.reg(clientRead[i]);
        kq.reg(clientWrite[i]);

        workers.emplace_back(
            [](FdObj& fdoRead, FdObj& fdoWrite) {
                while (true)
                {
                    if (!operate(fdoRead.getFd(), QUERY_TEXT, read))
                        break;
                    operate(fdoWrite.getFd(), RESPONSE_TEXT, write);
                }
            },
            std::ref(workerRead[i]),
            std::ref(workerWrite[i]));
    }

    std::thread client([&kq] {
        while (true)
        {
            if (auto pipes = kq.wait(); pipes.empty())
                break;
            else
                for (auto p : pipes)
                {
                    if (p->isRead())
                        operate(p->getFd(), RESPONSE_TEXT, read);
                    else
                        operate(p->getFd(), QUERY_TEXT, write);

                    if (--p->getCount() == 0)
                    {
                        kq.unreg(*p);
                        close(p->getFd());
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
