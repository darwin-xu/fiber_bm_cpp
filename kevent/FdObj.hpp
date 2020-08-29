#include <vector>

class FdObj
{
public:
    FdObj(int fd, int count, bool read);

    FdObj(const FdObj&) = delete;

    FdObj& operator=(const FdObj&) = delete;

    FdObj(FdObj&&) = default;

    FdObj& operator=(FdObj&&) = default;

    int getFd() const;

    int& getCount();

    bool isRead() const;

private:
    int  _fd;
    int  _count;
    bool _read;
};

using FdVector = std::vector<FdObj>;
