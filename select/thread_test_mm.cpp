#include "../facility.hpp"
#include "../separated.hpp"

int main(int argc, char* argv[])
{
    auto start = std::chrono::steady_clock::now();

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes1(workers_num);

    ThreadVector workers;
    for (auto i = 0; i < workers_num; ++i)
    {
        workers.emplace_back(
            [requests_num](int rd, int wt) {
                for (auto n = 0; n < requests_num; ++n)
                {
                    readOrWrite(rd, QUERY_TEXT, read);
                    readOrWrite(wt, RESPONSE_TEXT, write);
                }
            },
            worker_read[i],
            worker_write[i]);
    }

    ThreadVector masters;
    for (auto i = 0; i < workers_num; ++i)
    {
        masters.emplace_back(
            [requests_num](int rd, int wt) {
                for (auto n = 0; n < requests_num; ++n)
                {
                    readOrWrite(wt, QUERY_TEXT, write);
                    readOrWrite(rd, RESPONSE_TEXT, read);
                }
            },
            master_read[i],
            master_write[i]);
    }

    for (auto& w : workers)
        w.join();
    for (auto& m : masters)
        m.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
