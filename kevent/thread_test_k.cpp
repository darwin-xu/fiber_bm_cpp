#include "../facility.hpp"
#include "../separated.hpp"
#include "../Kq.hpp"

class Fd
{
public:
    Fd(const Fd&) = delete;

    Fd& operator=(const Fd&) = delete;

    Fd(const Fd&& o)
        : _fd(o._fd)
        , _count(o._count)
        , _read(o._read)
    {
    }

    Fd& operator=(const Fd&& r)
    {
        _fd    = r._fd;
        _count = r._count;
        _read  = r._read;
        return *this;
    }

    Fd(int fd, int count, bool read)
        : _fd(fd)
        , _count(count)
        , _read(read)
    {
    }

    int getFd() const
    {
        return _fd;
    }

    int& getCount()
    {
        return _count;
    }

    bool isRead() const
    {
        return _read;
    }

private:
    int  _fd;
    int  _count;
    bool _read;
};

using FdVector = std::vector<Fd>;

int main(int argc, char* argv[])
{
    Kq<Fd> kq;

    auto workers_num  = std::stoi(argv[1]);
    auto requests_num = std::stoi(argv[2]);

    FdVector     worker_read;
    FdVector     worker_write;
    FdVector     master_read;
    FdVector     master_write;
    ThreadVector workers;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < workers_num; ++i)
    {
        int p1[2], p2[2];
        assert(pipe(p1) == 0);
        assert(pipe(p2) == 0);

        worker_read.emplace_back(p1[0], requests_num, true);
        master_write.emplace_back(p1[1], requests_num, false);
        master_read.emplace_back(p2[0], requests_num, true);
        worker_write.emplace_back(p2[1], requests_num, false);

        kq.regRead(master_read[i]);
        kq.regWrite(master_write[i]);

        workers.emplace_back(
            [requests_num](Fd& fdRead, Fd& fdWrite) {
                for (int n = 0; n < requests_num; ++n)
                {
                    readOrWrite(fdRead.getFd(), QUERY_TEXT, read);
                    readOrWrite(fdWrite.getFd(), RESPONSE_TEXT, write);
                }
            },
            std::ref(worker_read[i]),
            std::ref(worker_write[i]));
    }

    std::thread master([&kq]() {
        while (true)
        {
            auto fds = kq.wait();
            if (fds.empty())
                break;
            for (auto fd : fds)
            {
                if (fd->isRead())
                    readOrWrite(fd->getFd(), RESPONSE_TEXT, read);
                else
                {
                    readOrWrite(fd->getFd(), QUERY_TEXT, write);
                    if (--fd->getCount() == 0)
                        kq.unreg(*fd);
                }
            }
        }
    });

    for (auto& w : workers)
        w.join();
    master.join();

    auto end = std::chrono::steady_clock::now();

    printStat(start, end, static_cast<double>(workers_num * requests_num));

    return 0;
}
