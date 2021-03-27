#include "FdObj.hpp"

std::atomic_long FdObj::_yieldCount;

FdObj::FdObj(int fd, int count, bool read)
    : _fd(fd)
    , _count(count)
    , _read(read)
{
}

int FdObj::getFd() const
{
    return _fd;
}

int& FdObj::getCount()
{
    return _count;
}

bool FdObj::isRead() const
{
    return _read;
}

void FdObj::resume()
{
    _promise->set_value(0);
}

void FdObj::yield()
{
    ++_yieldCount;
    _promise = std::make_unique<boost::fibers::promise<int>>();
    _promise->get_future().get();
}

long FdObj::getYieldCount()
{
    return _yieldCount.load();
}
