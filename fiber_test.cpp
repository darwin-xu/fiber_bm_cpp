#include <boost/fiber/all.hpp>

#include "facility.hpp"
#include "separated.hpp"

class Flag;

using Int2FlagMap = std::map<int, Flag*>;

class Flag
{
public:
    void wait(int fd, Int2FlagMap& ifm)
    {
        ifm[fd] = this;
        std::unique_lock<boost::fibers::mutex> l(*m);
        c->wait(
            l,
            [this]() -> auto { return f; });
        f = false;
    }

    void notify(int fd, Int2FlagMap& ifm)
    {
        ifm.erase(fd);
        if (!f)
        {
            f = true;
            c->notify_one();
            boost::this_fiber::yield();
        }
    }

private:
    bool                                               f = false;
    std::unique_ptr<boost::fibers::mutex>              m = std::make_unique<boost::fibers::mutex>();
    std::unique_ptr<boost::fibers::condition_variable> c = std::make_unique<boost::fibers::condition_variable>();
};

int main(int argc, char* argv[])
{
    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    IntVector worker_read;
    IntVector worker_write;
    IntVector master_read;
    IntVector master_write;

    for (int i = 0; i < workers_num; ++i)
    {
        int fildes[2];

        assert(pipe(fildes) == 0);
        worker_read.push_back(fildes[0]);
        master_write.push_back(fildes[1]);

        assert(pipe(fildes) == 0);
        master_read.push_back(fildes[0]);
        worker_write.push_back(fildes[1]);
    }

    using FiberVector = std::vector<boost::fibers::fiber>;
    using FlagVector  = std::vector<Flag>;

    FiberVector fv;
    FlagVector  fvRead;
    FlagVector  fvWrite;
    Int2FlagMap ifmRead;
    Int2FlagMap ifmWrite;

    fvRead.resize(workers_num);
    fvWrite.resize(workers_num);

    for (int i = 0; i < workers_num; ++i)
    {
        fv.emplace_back([i, requests_num, &worker_read, &worker_write, &fvRead, &fvWrite, &ifmRead, &ifmWrite]() {
            for (int n = 0; n < requests_num; ++n)
            {
                fvRead[i].wait(worker_read[i], ifmRead);
                readOrWrite(worker_read[i], QUERY_TEXT, read);

                fvWrite[i].wait(worker_write[i], ifmWrite);
                readOrWrite(worker_write[i], RESPONSE_TEXT, write);
            }
        });
    }

    boost::fibers::fiber reactorFiber([&worker_read, &worker_write, &ifmRead, &ifmWrite]() {
        while (true)
        {
            auto map2vector = [](const Int2FlagMap& ifm) {
                IntVector iv;
                for (auto& a : ifm)
                    iv.push_back(a.first);
                return iv;
            };

            if (ifmRead.empty() && ifmWrite.empty())
                break;

            auto [readable, writeable] = sselect(map2vector(ifmRead), map2vector(ifmWrite));

            auto notifyAndYield = [](const IntVector& iv, Int2FlagMap& ifm) {
                for (auto fd : iv)
                    ifm[fd]->notify(fd, ifm);
            };
            notifyAndYield(readable, ifmRead);
            notifyAndYield(writeable, ifmWrite);
        }
    });

    auto start = std::chrono::steady_clock::now();

    std::thread mt([workers_num, requests_num, &master_read, &master_write]() {
        auto pendingItems = workers_num * requests_num;
        while (pendingItems > 0)
        {
            auto [readable, writeable] = sselect(master_read, master_write);

            for (auto fd : readable)
                readOrWrite(fd, RESPONSE_TEXT, read);
            for (auto fd : writeable)
            {
                // std::cout << "write to: " << fd << std::endl;
                readOrWrite(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& f : fv)
        f.join();
    reactorFiber.join();

    mt.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}

int zmain(int argc, char* argv[])
{
    boost::fibers::mutex              mtx;
    boost::fibers::condition_variable cond;

    boost::fibers::fiber f2([&mtx, &cond]() {
        for (int i = 0; i < 5; ++i)
        {
            cond.notify_one();
            std::cout << "f2" << std::endl;
            boost::this_fiber::yield();
        }
    });

    boost::fibers::fiber f1([&mtx, &cond]() {
        for (int i = 0; i < 5; ++i)
        {
            std::unique_lock<boost::fibers::mutex> lk(mtx);
            std::cout << "f1 {" << std::endl;
            cond.wait(lk);
            std::cout << "f1 }" << std::endl;
        }
    });

    f1.join();
    f2.join();

    return 0;
}
