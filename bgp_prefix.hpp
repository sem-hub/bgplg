#ifndef _BGP_PREFIX_HPP
#define _BGP_PREFIX_HPP
#include <string>
#include <stdexcept>
#include <sys/types.h>
#include "ip_addr.hpp"

class bgp_prefix {
    private:
	ip_addr	net;
	u_char	len;
    public:
	bgp_prefix() : net(0), len(0) {}
	bgp_prefix(in_addr_t _net, u_char _len) : net(_net), len(_len) {}
	bgp_prefix(const u_char *buf) throw(std::invalid_argument) { parse(buf); }
	void parse(const u_char *buf) throw(std::invalid_argument);
	std::string as_string();
	ip_addr get_net() { return net; }
	u_char get_len() { return len; }
};

#endif
