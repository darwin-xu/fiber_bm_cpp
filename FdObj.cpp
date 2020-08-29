#include "FdObj.hpp"

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

void FdObj::notify()
{
    _promise->set_value(0);
}

void FdObj::wait()
{
    _promise = std::make_unique<boost::fibers::promise<int>>();
    auto f   = _promise->get_future();
    f.get();
}
