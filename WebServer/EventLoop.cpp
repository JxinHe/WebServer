// @Author Lin Ya 
// @Email xxbbb@vip.qq.com
#include "EventLoop.h"
#include "base/Logging.h"
#include "Util.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <iostream>
using namespace std;

__thread EventLoop* t_loopInThisThread = 0;

int createEventfd()//创建一个文件描述符用于事件通知
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);//创建一个文件描述符用于事件通知，由内核通知用户空间应用程序事件
    if (evtfd < 0)//NONBLOCK代表非阻塞，ClOEXEC，创建子进程时不继承父进程的文件描述符
    {
        LOG << "Failed in eventfd";
        abort();//创建失败则终止程序
    }
    return evtfd;//否则返回该文件描述符
}

EventLoop::EventLoop() //事件循环对象
:   looping_(false),
    poller_(new Epoll()),
    wakeupFd_(createEventfd()),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    pwakeupChannel_(new Channel(this, wakeupFd_))  //创建活跃的事件分发器分发刚刚活跃的文件描述符？？
{
    if (t_loopInThisThread)  //是否在本线程中有其他线程
    {
        //LOG << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET); //设置可读和水平触发
        //（epoll_event 结构体的events字段是表示感兴趣的事件和被触发的事件可能的取值为：EPOLLIN ：表示对应的文件描述符可以读）
   
    pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));//绑定回调读函数
    pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));//绑定回调XX函数？？？？（应该是处理tcpConnection函数）
    poller_->epoll_add(pwakeupChannel_, 0);//回调调用？？
}

void EventLoop::handleConn()
{
    //poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET | EPOLLONESHOT), 0);
    updatePoller(pwakeupChannel_, 0);
}


EventLoop::~EventLoop()
{
    //wakeupChannel_->disableAll();
    //wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = NULL;
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one)
    {
        LOG<< "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = readn(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}

void EventLoop::runInLoop(Functor&& cb)
{
    if (isInLoopThread())
        cb();
    else
        queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor&& cb)
{
    {
        MutexLockGuard lock(mutex_);//好像是跨线程调用，要加锁，忘了。。？？
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}

void EventLoop::loop()
{
    assert(!looping_);
    assert(isInLoopThread());
    looping_ = true;
    quit_ = false;
    //LOG_TRACE << "EventLoop " << this << " start looping";
    std::vector<SP_Channel> ret;
    while (!quit_)
    {
        //cout << "doing" << endl;
        ret.clear();
        ret = poller_->poll();
        eventHandling_ = true;
        for (auto &it : ret)
            it->handleEvents();
        eventHandling_ = false;
        doPendingFunctors();
        poller_->handleExpired();
    }
    looping_ = false;
}

void EventLoop::doPendingFunctors()//？？什么作用
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i)
        functors[i]();
    callingPendingFunctors_ = false;
}

void EventLoop::quit()//退出？？
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}
