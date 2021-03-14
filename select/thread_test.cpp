#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes1(workers_num);

    ThreadVector workers;
    for (auto i = 0; i < workers_num; ++i)
    {
        // Using something like this will cause compile error:
        // workers.emplace_back([i, requests_num, &worker_read, &worker_write]
        // it is an issue of C++ std:
        // http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#2313
        // https://stackoverflow.com/questions/46114214/lambda-implicit-capture-fails-with-variable-declared-from-structured-binding
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

    // "Captureing with the initializer" is a workaround.
    std::thread mt([wn  = workers_num,
                    rn  = requests_num,
                    mrd = master_read,
                    mwt = master_write] {
        auto pendingItems = wn * rn;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(mrd, mwt);

            for (auto fd : readable)
                operate(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                operate(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& w : workers)
        w.join();
    mt.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
