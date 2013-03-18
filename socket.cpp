#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>
#include <netinet/in.h>
#include "socket.hpp"

using namespace std;

Socket::Socket() : created_(false), type_(0), terminate_(false)
{
}

Socket::Socket(const Socket& s) : s_(s.s_), created_(true), terminate_(false)
{
}

Socket::Socket(int s) : s_(s), created_(true), terminate_(false)
{
}

void
Socket::close()
{
    if(created_)
	::close(s_);
}

void
Socket::create(int type)
{
    type_ = type;
    s_ = socket(AF_INET, type, 0);
    if(s_ < 0)
	throw SocketOsError("socket", errno);
    created_ = true;
}

void
Socket::listen(in_addr_t bindip, in_port_t bindport)
{
    int n = 1;
    struct sockaddr_in  sin;

    if(!created_)
	throw SocketNotCreated();

    if(::setsockopt(s_, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n)) == -1)
	throw SocketOsError("setsockopt", errno);
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = bindip;
    sin.sin_port = bindport;

    if (::bind(s_, (struct sockaddr *)&sin, sizeof(sin)) == -1)
	throw SocketOsError("bind", errno);

    if (::listen(s_, 1024) == -1)
	throw SocketOsError("listen", errno);
}

bool
Socket::is_ready()
{
    fd_set	fdset;
    struct timeval tm = { 0, 100 };

    if(!created_)
	throw SocketNotCreated();

    FD_ZERO(&fdset);
    FD_SET(s_, &fdset);
    if((select(s_ + 1, &fdset, NULL, NULL, &tm)) > 0 && FD_ISSET(s_, &fdset))
	return true;

    return false;
}

Socket*
Socket::accept()
{
    struct sockaddr_in  claddr;
    socklen_t len = sizeof(claddr);
    int newsocket;

    if(!created_)
	throw SocketNotCreated();

    if((newsocket=::accept(s_, (struct sockaddr *)&claddr, &len)) < 0)
	throw SocketOsError("accept", errno);

    return new Socket(newsocket);
}

size_t
Socket::read(char *buf, size_t size)
{
    size_t s = ::recv(s_, buf, size, 0);
    if(s < 0)
	throw SocketOsError("recv", errno);

    return s;
}

void
Socket::read_assure(char *buf, size_t size)
{
    size_t m = 0;
    ssize_t n;

    if(!created_)
	throw SocketNotCreated();

    do {
	n = ::recv(s_, buf+m, size-m, 0);
	if(n < 0 && errno != EAGAIN)
	    throw SocketNotEnoughData();
	m += n;
    } while(m < size && !terminate_);
}

void
Socket::write(const char *buf, size_t size)
{
    if(!created_)
	throw SocketNotCreated();

    if(::write(s_, buf, size) < (ssize_t)size)
	throw SocketOsError("write", errno);
}

in_addr_t
Socket::get_peer()
{
    struct sockaddr_in peername;
    socklen_t len = sizeof(peername);

    if(!created_)
	throw SocketNotCreated();

    if(::getpeername(s_, (struct sockaddr*)&peername, &len) < 0) {
	// XXX cout << "errno=" << errno << endl;
	throw SocketOsError("getpeername", errno);
    }

    return ntohl(peername.sin_addr.s_addr);
}
