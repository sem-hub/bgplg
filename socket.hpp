#ifndef _SOCKET_HPP
#define _SOCKET_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>
#include "exceptions.hpp"

class Socket {
    public:
	Socket();
	Socket(const Socket& s);
	void create(int type);
	size_t read(char *buf, size_t size);
	void read_assure(char *buf, size_t size);
	void write(const char *buf, size_t size);
	void listen(in_addr_t bindip, in_port_t bindport);
	void close();
	bool is_ready();
	Socket* accept();
	in_addr_t get_peer();
	void terminate() { terminate_ = true; }
    private:
	Socket(int s);
	int s_;
	bool created_;
	int type_;
	bool terminate_;
};

class SocketNotCreated : public GenericException {
    public:
	SocketNotCreated() : GenericException("socket is not created") {}
};

class SocketOsError : public GenericException {
    public:
	SocketOsError(std::string funcName, int eno) {
	    std::ostringstream s;
	    s << "Function execution error: " << funcName << "(); errno=" << eno;
	    GenericException(s.str());
	}
};

class SocketNotEnoughData : public GenericException {
    public:
	SocketNotEnoughData() : GenericException("Read not enough data from socket") {}
};
#endif
