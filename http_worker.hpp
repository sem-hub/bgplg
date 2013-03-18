#ifndef _HTTP_WORKER_H
#define _HTTP_WORKER_H

#include "thread.hpp"
#include "ip_addr.hpp"

class Socket;

class http_worker: public Thread {
    public:
	http_worker() {}
	~http_worker() {}
    protected:
	virtual void Execute(void *);
    private:
	ip_addr peer_;
	Socket *s_;
};

#endif
