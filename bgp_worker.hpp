#ifndef _BGP_WORKER_H
#define _BGP_WORKER_H

#include <time.h>
#include "thread.hpp"
#include "exceptions.hpp"
#include "ip_addr.hpp"

class Socket;
struct msg_header;

class bgp_worker: public Thread {
    public:
	bgp_worker() : stop_(false), s_(NULL), peer_(0), hold_time(0), last_keepalive(0) {}
	~bgp_worker() {};
    protected:
	virtual void Execute(void *);
    private:
	void process_open(const struct msg_header&);
	void process_update(const struct msg_header&);
	void process_notification(const struct msg_header&);
	void send_keepalive();
	void stop() { stop_ = true; s_->terminate(); }
	void end_session();

	void process_withdraw(const char*);

	bool	stop_;
	Socket	*s_;
	ip_addr	peer_;
	time_t	hold_time;
	time_t	last_keepalive;
};

class BadAlloc : public GenericException {
    public:
	BadAlloc() : GenericException("malloc error") {}
};

#endif
