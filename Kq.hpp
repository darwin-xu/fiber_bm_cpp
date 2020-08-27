#include <vector>

#ifdef MACOS
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#ifdef LINUX
#include <sys/epoll.h>
#endif

template<class T>
class Kq
{
public:
    using TVector = std::vector<T*>;

    void regRead(T& t)
    {
#ifdef MACOS
        reg(t, EVFILT_READ);
#endif
#ifdef LINUX
        reg(t, EPOLLIN);
#endif
    }

    void regWrite(T& t)
    {
#ifdef MACOS
        reg(t, EVFILT_WRITE);
#endif
#ifdef LINUX
        reg(t, EPOLLOUT);
#endif
    }

    void unreg(T& t)
    {
#ifdef MACOS
        struct kevent event;
        EV_SET(&event, t.getFd(), t.isRead() ? EVFILT_READ : EVFILT_WRITE, EV_DELETE, 0, 0, &t);
        assert(kevent(_kq, &event, 1, NULL, 0, NULL) != -1);
#endif
#ifdef LINUX
        struct epoll_event event;
        event.events   = EPOLLIN | EPOLLOUT;
        event.data.ptr = &t;
        assert(epoll_ctl(_ep, EPOLL_CTL_DEL, t.getFd(), &event) != -1);
#endif
    }

    TVector wait()
    {
        TVector       tv;
        constexpr int EVENT_SIZE = 1024;
#ifdef MACOS
        struct kevent events[EVENT_SIZE];
        auto          e = kevent(_kq, NULL, 0, events, EVENT_SIZE, NULL);
        assert(e != -1);
        for (int i = 0; i < e; ++i)
            tv.push_back(reinterpret_cast<T*>(events[i].udata));
#endif
#ifdef LINUX
        struct epoll_event events[EVENT_SIZE];
        auto               e = epoll_wait(_ep, events, EVENT_SIZE, -1);
        assert(e != -1);
        for (int i = 0; i < e; ++i)
            tv.push_back(reinterpret_cast<T*>(events[i].data.ptr));
#endif
        return tv;
    }

private:
    void reg(T& t, int filter)
    {
#ifdef MACOS
        struct kevent event;
        EV_SET(&event, t.getFd(), filter, EV_ADD | EV_CLEAR, 0, 0, &t);
        assert(kevent(_kq, &event, 1, NULL, 0, NULL) != -1);
#endif
#ifdef LINUX
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = &t;
        assert(epoll_ctl(_ep, EPOLL_CTL_ADD, t.getFd(), &event) != -1);
#endif
    }

#ifdef MACOS
    int _kq = kqueue();
#endif
#ifdef LINUX
    int _ep = epoll_create(1);
#endif
};