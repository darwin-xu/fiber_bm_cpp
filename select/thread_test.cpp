#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [clientsNumber, requestsNumber] =
        parseArg2(argc, argv, "<clients number> <requests number>");

    auto [workerRead, workerWrite, clientRead, clientWrite] =
        initPipes1(clientsNumber);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    TPFutureVector wfs;
    for (auto i = 0; i < clientsNumber; ++i)
    {
        // Using something like this will cause compile error:
        // workers.emplace_back([i, requests_num, &worker_read, &worker_write]
        // "Capturing with the initializer" is a workaround.
        // it is an issue of C++ std:
        // http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#2313
        // https://stackoverflow.com/questions/46114214/lambda-implicit-capture-fails-with-variable-declared-from-structured-binding
        auto worker = [](int rd, int wt) {
            while (true)
            {
                if (!operate(rd, QUERY_TEXT, read))
                    break;
                operate(wt, RESPONSE_TEXT, write);
            }
            close(rd);
            close(wt);

            return std::chrono::steady_clock::now();
        };
        wfs.push_back(std::async(std::launch::async,
                                 worker,
                                 workerRead[i],
                                 workerWrite[i]));
    }

    auto client =
        [rn = requestsNumber, rds = clientRead, wts = clientWrite]() mutable {
            Int2IntMap pendingWrite;
            for (auto fd : wts)
                pendingWrite[fd] = rn;

            while (!rds.empty())
            {
                auto [readable, writeable] = sselect(rds, wts);

                auto removeFD = [](IntVector& iv, int fd) {
                    iv.erase(std::remove(iv.begin(), iv.end(), fd), iv.end());
                };

                for (auto fd : readable)
                {
                    if (!operate(fd, RESPONSE_TEXT, read))
                        removeFD(rds, fd);
                }
                for (auto fd : writeable)
                {
                    operate(fd, QUERY_TEXT, write);
                    if (--pendingWrite[fd] == 0)
                    {
                        removeFD(wts, fd);
                        close(fd);
                    }
                }
            }

            return std::chrono::steady_clock::now();
        };
    auto cf = std::async(std::launch::async, client);

    STPVector stpv;
    for (auto& wf : wfs)
        stpv.emplace_back("worker", wf.get());

    stpv.emplace_back("client", cf.get());

    // 3. Output statistics
    printStat(clientsNumber * requestsNumber, start, stpv);

    return 0;
}
