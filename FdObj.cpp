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
