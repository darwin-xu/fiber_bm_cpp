#include "FdObj.hpp"

FdObj::FdObj(int fd, int count, bool read)
    : _fd(fd)
    , _count(count)
    , _read(read)
{
}

FdObj::FdObj(const FdObj&& o)
    : _fd(o._fd)
    , _count(o._count)
    , _read(o._read)
{
}

FdObj& FdObj::operator=(const FdObj&& r)
{
    _fd    = r._fd;
    _count = r._count;
    _read  = r._read;
    return *this;
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
