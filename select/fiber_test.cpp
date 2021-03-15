#include <boost/fiber/all.hpp>

#include "../facility.hpp"
#include "../separated.hpp"

class Flag;

using Int2FlagMap = std::map<int, Flag*>;

class Flag
{
public:
    void wait(int fd, Int2FlagMap& ifm)
    {
        ifm[fd] = this;
        auto l  = std::unique_lock(*m);
        // std::unique_lock<boost::fibers::mutex> l(*m);
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
    bool                                  f = false;
    std::unique_ptr<boost::fibers::mutex> m =
        std::make_unique<boost::fibers::mutex>();
    std::unique_ptr<boost::fibers::condition_variable> c =
        std::make_unique<boost::fibers::condition_variable>();
};

int main(int argc, char* argv[])
{
    // 1. Preparation
    auto [workers_num, requests_num] =
        parseArg2(argc, argv, "<workers number> <requests number>");

    auto [worker_read, worker_write, master_read, master_write] =
        initPipes1(workers_num);

    using FiberVector = std::vector<boost::fibers::fiber>;
    using FlagVector  = std::vector<Flag>;

    FiberVector fv;
    FlagVector  fvRead;
    FlagVector  fvWrite;
    Int2FlagMap ifmRead;
    Int2FlagMap ifmWrite;

    fvRead.resize(workers_num);
    fvWrite.resize(workers_num);

    // 2. Start evaluation
    auto start = std::chrono::steady_clock::now();

    for (auto i = 0; i < workers_num; ++i)
    {
        fv.emplace_back([i,
                         rn  = requests_num,
                         wrd = worker_read,
                         wwt = worker_write,
                         &fvRead,
                         &fvWrite,
                         &ifmRead,
                         &ifmWrite] {
            for (auto n = 0; n < rn; ++n)
            {
                fvRead[i].wait(wrd[i], ifmRead);
                operate(wrd[i], QUERY_TEXT, read);

                fvWrite[i].wait(wwt[i], ifmWrite);
                operate(wwt[i], RESPONSE_TEXT, write);
            }
        });
    }

    boost::fibers::fiber reactorFiber([&ifmRead, &ifmWrite] {
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

            auto [readable, writeable] =
                sselect(map2vector(ifmRead), map2vector(ifmWrite));

            auto notifyAndYield = [](const IntVector& iv, Int2FlagMap& ifm) {
                for (auto fd : iv)
                    ifm[fd]->notify(fd, ifm);
            };
            notifyAndYield(readable, ifmRead);
            notifyAndYield(writeable, ifmWrite);
        }
    });

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

    for (auto& f : fv)
        f.join();
    reactorFiber.join();

    mt.join();

    auto end = std::chrono::steady_clock::now();

    // 3. Output statistics
    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
