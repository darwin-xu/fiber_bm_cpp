#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes1(workers_num);

    ThreadVector workers;
    for (int i = 0; i < workers_num; ++i)
    {
        // Using something like this will cause compile error:
        // workers.emplace_back([i, requests_num, &worker_read, &worker_write]()
        // it is an issue of C++ std: http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#2313
        // https://stackoverflow.com/questions/46114214/lambda-implicit-capture-fails-with-variable-declared-from-structured-binding
        workers.emplace_back(
            [i, requests_num](int rd, int wt) {
                for (int n = 0; n < requests_num; ++n)
                {
                    readOrWrite(rd, QUERY_TEXT, read);
                    readOrWrite(wt, RESPONSE_TEXT, write);
                }
            },
            worker_read[i],
            worker_write[i]);
    }

    auto start = std::chrono::steady_clock::now();

    // "Captureing with the initializer" is a workaround.
    std::thread mt([workers_num, requests_num, mrd = master_read, mwt = master_write]() {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(mrd, mwt);

            for (auto fd : readable)
                readOrWrite(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                readOrWrite(fd, QUERY_TEXT, write);
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
