#include <boost/fiber/all.hpp>

#include "facility.hpp"
#include "separated.hpp"

class FiberFlag
{
public:
    void wait()
    {
        std::unique_lock<boost::fibers::mutex> l(*m);
        c->wait(
            l,
            [this]() -> auto { return f; });
        f = false;
    }

    void notify()
    {
        std::unique_lock<boost::fibers::mutex> l(*m);
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
    using MutexVector = std::vector<std::unique_ptr<boost::fibers::mutex>>;
    using CondVector  = std::vector<std::unique_ptr<boost::fibers::condition_variable>>;
    using IntContMap  = std::map<int, boost::fibers::condition_variable*>;
    using FFVector    = std::vector<FiberFlag>;
    using IntFFMap    = std::map<int, FiberFlag*>;

    FiberVector fv;
    MutexVector mv;
    CondVector  cvRead;
    CondVector  cvWrite;
    IntContMap  rd2Cond;
    IntContMap  wt2Cond;
    FFVector    ffvRead;
    FFVector    ffvWrite;
    IntFFMap    rd2ff;
    IntFFMap    wt2ff;

    ffvRead.resize(workers_num);
    ffvWrite.resize(workers_num);

    for (int i = 0; i < workers_num; ++i)
    {
        mv.emplace_back(std::make_unique<boost::fibers::mutex>());
        cvRead.emplace_back(std::make_unique<boost::fibers::condition_variable>());
        cvWrite.emplace_back(std::make_unique<boost::fibers::condition_variable>());
        rd2Cond[worker_read[i]]  = cvRead[i].get();
        wt2Cond[worker_write[i]] = cvWrite[i].get();
        rd2ff[worker_read[i]]    = &ffvRead[i];
        wt2ff[worker_write[i]]   = &ffvWrite[i];

        fv.emplace_back([i, requests_num, &mv, &cvRead, &cvWrite, &worker_read, &worker_write, &ffvRead, &ffvWrite]() {
            for (int n = 0; n < requests_num; ++n)
            {
                std::cout << "f " << i << "-" << n << ", wait read on " << worker_read[i] << std::endl;
                // std::unique_lock<boost::fibers::mutex> l(*mv[i]);
                // cvRead[i]->wait(l);
                // readOrWrite(worker_read[i], QUERY_TEXT, read);
                // cvWrite[i]->wait(l);
                // readOrWrite(worker_write[i], RESPONSE_TEXT, write);
                ffvRead[i].wait();
                readOrWrite(worker_read[i], QUERY_TEXT, read);

                std::cout << "f " << i << "-" << n << ", wait write on " << worker_write[i] << std::endl;
                ffvWrite[i].wait();
                readOrWrite(worker_write[i], RESPONSE_TEXT, write);
            }
        });
    }

    boost::fibers::fiber reactorFiber([&rd2Cond, &wt2Cond, &worker_read, &worker_write, &rd2ff, &wt2ff]() {
        auto [readable, writeable] = sselect(worker_read, worker_write);

        auto notifyAndYield = [](const IntVector& iv, IntFFMap& ifm) {
            for (auto fd : iv)
            {
                std::cout << "r" << fd << " " << std::endl;
                ifm[fd]->notify();
            }
        };
        notifyAndYield(readable, rd2ff);
        notifyAndYield(writeable, wt2ff);
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
                readOrWrite(fd, QUERY_TEXT, write);
                --pendingItems;
            }
        }
    });

    for (auto& f : fv)
        f.join();

    mt.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    // reactorFiber.join();

    return 0;
}

int xmain(int argc, char* argv[])
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
