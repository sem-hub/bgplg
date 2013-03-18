#include <sstream>
#include <stdexcept>
#include <cmath>
#include <sys/types.h>
#include "bgp_prefix.hpp"

using namespace std;

void
bgp_prefix::parse(const u_char *buf) throw(invalid_argument)
{
    in_addr_t n=0;

    /* the first byte is a prefix len (bits) */
    if(buf[0] == 0 || buf[0] > 32)
	throw invalid_argument("bad mask len: %d"+(int)*(uint8_t*)buf);
    len = buf[0];

    for(int i=0; i<ceil(len/8.); i++)
	n |= buf[i+1] << i*8;

    net.set_addr(n);
}

string
bgp_prefix::as_string()
{
    ostringstream ss;

    ss << net.as_string() << "/" << (int)len;
    return ss.str();
}
