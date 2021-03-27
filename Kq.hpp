#ifndef KQ_H
#define KQ_H

#include <vector>
#include <unordered_set>

#if defined(MACOS) || defined(FREEBSD)
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

    void reg(T& t)
    {
        if (t.isRead())
        {
#if defined(MACOS) || defined(FREEBSD)
            reg(t, EVFILT_READ);
#endif
#ifdef LINUX
            reg(t, EPOLLIN);
#endif
        }
        else
        {
#if defined(MACOS) || defined(FREEBSD)
            reg(t, EVFILT_WRITE);
#endif
#ifdef LINUX
            reg(t, EPOLLOUT);
#endif
        }
    }

    void unreg(T& t)
    {
        fdSet.erase(t.getFd());
#if defined(MACOS) || defined(FREEBSD)
        struct kevent event;
        EV_SET(&event,
               t.getFd(),
               t.isRead() ? EVFILT_READ : EVFILT_WRITE,
               EV_DELETE,
               0,
               0,
               &t);
        auto r = kevent(_kq, &event, 1, NULL, 0, NULL);
        assert(r != -1 && "kevent() function fails.");
#endif
#ifdef LINUX
        struct epoll_event event;
        event.events   = EPOLLIN | EPOLLOUT;
        event.data.ptr = &t;
        auto r         = epoll_ctl(_ep, EPOLL_CTL_DEL, t.getFd(), &event);
        assert(r != -1 && "epoll_ctl() function fails.");
#endif
    }

    TVector wait()
    {
#ifndef NDEBUG
        ++_waitCount;
#endif

        TVector       tv;
        constexpr int EVENT_SIZE = 1024;
        if (!fdSet.empty())
        {
#if defined(MACOS) || defined(FREEBSD)
            struct kevent events[EVENT_SIZE];
            auto          e = kevent(_kq, NULL, 0, events, EVENT_SIZE, NULL);
            assert(e != -1 && "kevent() function fails.");
            for (auto i = 0; i < e; ++i)
                tv.push_back(reinterpret_cast<T*>(events[i].udata));
#endif
#ifdef LINUX
            struct epoll_event events[EVENT_SIZE];
            auto               e = epoll_wait(_ep, events, EVENT_SIZE, -1);
            assert(e != -1 && "epoll_wait() function fails.");
            for (auto i = 0; i < e; ++i)
                tv.push_back(reinterpret_cast<T*>(events[i].data.ptr));
#endif
        }
        return tv;
    }

#ifndef NDEBUG
    long getWaitCount()
    {
        return _waitCount;
    }
#endif

private:
    void reg(T& t, int filter)
    {
        fdSet.insert(t.getFd());
#if defined(MACOS) || defined(FREEBSD)
        struct kevent event;
        EV_SET(&event, t.getFd(), filter, EV_ADD | EV_CLEAR, 0, 0, &t);
        auto r = kevent(_kq, &event, 1, NULL, 0, NULL);
        assert(r != -1 && "kevent() function fails.");
#endif
#ifdef LINUX
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = &t;
        auto r         = epoll_ctl(_ep, EPOLL_CTL_ADD, t.getFd(), &event);
        assert(r != -1 && "epoll_ctl() function fails.");
#endif
    }

#if defined(MACOS) || defined(FREEBSD)
    int _kq = kqueue();
#endif
#ifdef LINUX
    int _ep = epoll_create(1);
#endif
    std::unordered_set<int> fdSet;

#ifndef NDEBUG
    long _waitCount = 0;
#endif
};

#endif // KQ_H
