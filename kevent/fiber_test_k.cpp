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

    FdVector worker_read;
    FdVector worker_write;
    FdVector master_read;
    FdVector master_write;

    for (int i = 0; i < workers_num; ++i)
    {
        int p1[2], p2[2];
        assert(pipe(p1) == 0);
        assert(pipe(p2) == 0);

        worker_read.emplace_back(p1[0], requests_num, true);
        master_write.emplace_back(p1[1], requests_num, false);
        master_read.emplace_back(p2[0], requests_num, true);
        worker_write.emplace_back(p2[1], requests_num, false);
    }

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
