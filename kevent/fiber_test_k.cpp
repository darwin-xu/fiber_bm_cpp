#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"
#include "../FdObj.hpp"

using FiberVector = std::vector<boost::fibers::fiber>;

int main(int argc, char* argv[])
{
    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    auto [worker_read, worker_write, master_read, master_write] = initPipes2(workers_num, requests_num);

    FiberVector fv;
    Kq<FdObj>   kq;

    for (int i = 0; i < workers_num; ++i)
    {
        fv.emplace_back(
            [requests_num](FdObj& read, FdObj& write) {
                for (int n = 0; n < requests_num; ++n)
                {
                }
            },
            std::ref(worker_read[i]),
            std::ref(worker_write[i]));
    }

    return 0;
}
