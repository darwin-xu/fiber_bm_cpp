#ifndef FDOBJ_H
#define FDOBJ_H

#include <vector>
#include <memory>

#include <boost/fiber/all.hpp>

class FdObj
{
public:
    using Promise = std::unique_ptr<boost::fibers::promise<int>>;

    FdObj(int fd, int count, bool read);

    FdObj(const FdObj&) = delete;

    FdObj& operator=(const FdObj&) = delete;

    FdObj(FdObj&&) = default;

    FdObj& operator=(FdObj&&) = default;

    int getFd() const;

    int& getCount();

    bool isRead() const;

    void resume();

    void yield();

private:
    int     _fd;
    int     _count;
    bool    _read;
    Promise _promise;
};

using FdVector = std::vector<FdObj>;

#endif // FDOBJ_H
