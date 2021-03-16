#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num, batches_num] =
        parseArg3(argc,
                  argv,
                  "<workers number> <requests number> <batches number>");

    assert(requests_num % batches_num == 0);

    auto [worker_read, worker_write, client_read, client_write] =
        initPipes1(workers_num);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    ThreadVector workers;
    for (auto i = 0; i < workers_num; ++i)
    {
        workers.emplace_back(
            [rn = requests_num](int rd, int wt) {
                for (auto n = 0; n < rn; ++n)
                {
                    operate(rd, QUERY_TEXT, read);
                    operate(wt, RESPONSE_TEXT, write);
                }
            },
            worker_read[i],
            worker_write[i]);
    }

    ThreadVector clients;
    for (auto i = 0; i < workers_num; ++i)
    {
        clients.emplace_back(
            [rn = requests_num, bn = batches_num](int rd, int wt) {
                for (auto n = 0; n < rn / bn; ++n)
                {
                    for (int j = 0; j < bn; ++j)
                    {
                        operate(wt, QUERY_TEXT, write);
                        operate(rd, RESPONSE_TEXT, read);
                    }
                }
            },
            client_read[i],
            client_write[i]);
    }

    for (auto& w : workers)
        w.join();
    for (auto& c : clients)
        c.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
