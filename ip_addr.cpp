#include <string>
#include <sstream>
#include <stdexcept>
#include <arpa/inet.h>
#include "ip_addr.hpp"
using namespace std;

void
ip_addr::set_addr(in_addr_t ip, eorder etype)
{
    switch(etype) {
	case HOST_ORDER:
	    addr = htonl(ip);
	    break;
	case NET_ORDER:
	    addr = ip;
	    break;
    }
}

void
ip_addr::set_addr(const string& str)
{
    addr = 0;
    unsigned int octets[4];
    size_t pos1=0, pos2, i, dots=0;

    for(i=0; i < str.length(); i++) {
	if(str.at(i) == '.') {
	    dots++;
	    continue;
	}
	if(str.at(i) < '0' || str.at(i) > '9')
	    throw invalid_argument("illegal character");
    }
    if(dots != 3)
	throw invalid_argument("wrong address format");

    pos2 = str.find('.');
    i = 3;
    do {
	octets[i] = strtoul(str.substr(pos1, pos2).c_str(), NULL, 10);
	i--;
	pos1 = pos2+1;
	pos2 = str.find('.', pos1);
    } while(i>0);
    pos2 = str.size();
    octets[i] = strtoul(str.substr(pos1, pos2).c_str(), NULL, 10);

    for(i=0; i<4; i++) {
	if(octets[i] <= 255)
	    addr |= octets[i] << (i*8);
    	else
	    throw invalid_argument("octet is out of range");
    }
}

string
ip_addr::as_string() const
{
    ostringstream ss;

    for(int i=0; i<4; i++)
	ss << ((addr >> i*8) & 0xff) << (i<3?".":"");
    return ss.str();
}

in_addr_t
ip_addr::as_in_addr(eorder etype) const
{
    in_addr_t ret=addr;

    if(etype == HOST_ORDER)
	    ret = ntohl(ret);
    return ret;
}
