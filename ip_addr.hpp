#ifndef _IP_ADDR_HPP
#define _IP_ADDR_HPP
#include <sys/types.h>
#include <string>
#include <iostream>
#include <stdexcept>

typedef enum {
    HOST_ORDER = 0,
    NET_ORDER
} eorder;

class ip_addr {
    public:
	ip_addr() : addr(0) {}
	ip_addr(in_addr_t ip, eorder etype=NET_ORDER) { set_addr(ip, etype); }
	ip_addr(const std::string& str) { set_addr(str); }
	void set_addr(in_addr_t ip, eorder etype=NET_ORDER);
	void set_addr(const std::string& str);
	std::string as_string() const;
	in_addr_t as_in_addr(eorder etype=HOST_ORDER) const;
	bool operator==(ip_addr rhs) { return addr == rhs.addr; }
    private:
	/* Big-endian (network order) */
	in_addr_t addr;
};
#endif
