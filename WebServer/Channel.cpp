// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Channel.h"
#include "Util.h"
#include "Epoll.h"
#include "EventLoop.h"
#include <unistd.h>
#include <queue>
#include <cstdlib>
#include <iostream>
using namespace std;

Channel::Channel(EventLoop *loop):
    loop_(loop),
    events_(0),
    lastEvents_(0)
{ }

Channel::Channel(EventLoop *loop, int fd):
    loop_(loop),
    fd_(fd), 
    events_(0),
    lastEvents_(0)
{ }

Channel::~Channel()
{
    //loop_->poller_->epoll_del(fd, events_);
    //close(fd_);
}

int Channel::getFd()//得到fd
{
    return fd_;
}
void Channel::setFd(int fd)//设置fd
{
    fd_ = fd;
}

void Channel::handleRead()//处理read回调
{
    if (readHandler_)
    {
        readHandler_();
    }
}

void Channel::handleWrite()//处理write回调
{
    if (writeHandler_)
    {
        writeHandler_();
    }
}

void Channel::handleConn()
{
    if (connHandler_)
    {
        connHandler_();
    }
}
