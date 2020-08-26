#include <set>
#include <vector>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

template<class T>
class Kq
{
public:
    using TSet    = std::set<const T*>;
    using TVector = std::vector<T*>;

    void regRead(T& t)
    {
        reg(t, EVFILT_READ);
    }

    void regWrite(T& t)
    {
        reg(t, EVFILT_WRITE);
    }

    void unreg(T& t)
    {
        _tset.erase(&t);
        struct kevent event;
        EV_SET(&event, t.getFd(), EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, &t);
    }

    TVector wait()
    {
        TVector       tv;
        constexpr int EVENT_SIZE = 1024;
        struct kevent events[EVENT_SIZE];

        auto e = kevent(_kq, NULL, 0, events, EVENT_SIZE, NULL);
        assert(e != -1);
        for (int i; i < e; ++i)
            tv.push_back(reinterpret_cast<T*>(events[i].udata));
        return tv;
    }

private:
    void reg(T& t, short filter)
    {
        _tset.insert(&t);
        struct kevent event;
        EV_SET(&event, t.getFd(), EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, &t);
        assert(kevent(_kq, &event, 1, NULL, 0, NULL) != -1);
    }

    TSet _tset;
    int  _kq = kqueue();
};
