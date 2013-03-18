#include <time.h>
#include <sys/types.h>
#include <machine/atomic.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <stdexcept>
#include <log4cpp/Category.hh>
#include "global.hpp"
#include "socket.hpp"
#include "tcpstream.hpp"
#include "object_collection.hpp"
#include "bgp_attributes.hpp"
#include "http_worker.hpp"
#include "rwlock.hpp"
#include "ip_addr.hpp"

using namespace std;

class bgp_worker;

void
http_worker::Execute(void *arg)
{
    char buf[200], hname[50];
    string str, cmd="";
    in_addr_t net;
    uint8_t mask;
    struct sockaddr_in sa;
    list<bgp_attributes*> *data_list;
    bool found;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    s_ = reinterpret_cast<Socket*>(arg);
    TCPStream stream(*s_);
    peer_.set_addr(s_->get_peer(), HOST_ORDER);
    slog.infoStream() << "HTTP connected from: " << peer_.as_string() << eol;

    do {
	stream.getline(buf, sizeof(buf));
	str = buf;
	if(str.substr(0, 4) == "GET ") {
	    str.erase(0, 4);
	    cmd = str.substr(0, str.find(' '));
	}
	try {
	    str.replace(str.find('\r'), 1, "");
	    str.replace(str.find('\n'), 1, "");
	} catch(out_of_range) {
	}
    } while(str != "");

    slog.infoStream() << "Command: " << cmd << eol;

    if(cmd == "/status") {
	stream << "HTTP/1.0 200 OK\r\n";
	stream << "Content-Type: text/html\r\n\r\n";

	stream << "<h1>Statistic</h1>\r\n";
	stream << "peers connected: " << statistic.peers_number << "\r\n";
	stream << "<br>active prefixes: " << statistic.prefixes_number << "\r\n";
	stream << "<br>updates processed: " << statistic.updates_processed << "\r\n";
	stream << "<br>withdraw routes: " << statistic.withdraw_processed << "\r\n";
	stream << "<br>HTTP requests processed: " << statistic.http_requests << "\r\n";
	stream << "<br>uptime: " << time(NULL)-statistic.start_time << "\r\n";
	stream << flush;
    } else if(cmd == "/peers") {
	stream << "HTTP/1.0 200 OK\r\n";
	stream << "Content-Type: text/html\r\n\r\n";

	stream << "<h1>Peers:</h1>\r\n";

	bzero((void*)&sa, sizeof(sa));
	sa.sin_len = sizeof(struct sockaddr_in);
	sa.sin_family = AF_INET;
	{
	    /* accure a read lock for this block */
	    rwlock rlock(RLOCK, peers_lock);
	    for(list<pair<ip_addr, bgp_worker*> >::const_iterator ii=peers.begin();
		ii != peers.end(); ++ii) {
		    sa.sin_addr.s_addr = (*ii).first.as_in_addr(NET_ORDER);
		    ip_addr ip((*ii).first);
		    if(getnameinfo((struct sockaddr *)&sa, sa.sin_len, 
				hname, sizeof(hname), NULL, 0, NI_NAMEREQD) == 0)
			stream << hname << " (" << ip.as_string() << ")<br>";
		    else
			stream << ip.as_string() << "<br>";
	    }
	}
	stream << flush;
    } else {
	try {
	    cmd.replace(cmd.find('/'), 1, "");
	    /* if we have a mask */
	    if(cmd.find('/') != string::npos) {
		mask = strtol(cmd.substr(cmd.find('/')+1, cmd.length()).c_str(), NULL, 10);
		net = ntohl(inet_addr((cmd.substr(0, cmd.find('/')).c_str())));
	    } else {
		mask = 32;
		net = ntohl(inet_addr(cmd.c_str()));
	    }
	} catch(out_of_range) {
	    mask = 0;
	}
	if(net == INADDR_NONE || mask == 0 || mask > 32) {
	    stream << "HTTP/1.0 400 Bad Request\r\n";
	    stream << "Content-Type: text/html\r\n\r\n";
	    stream << "<h1>400 Bad Request</h1>\r\n" << flush;
	} else {
	    data_list = objs.get_nearest(net, mask);
	    stream << "HTTP/1.0 200 OK\r\n";
	    stream << "Content-Type: text/xml\r\n\r\n";
	    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n";
	    stream << "<result>\r\n";
	    found = false;
	    if(data_list) {
		stream << "  <prefix>" << (*data_list->begin())->get_prefix() << "</prefix>" << endl;
		for(list<bgp_attributes*>::const_iterator ii=data_list->begin();
			ii != data_list->end(); ++ii) {
		    stream << (*ii)->output_xml();
		    found = true;
		}
	    }
	    if(!found) {
		stream << "<error>Not found</error>\r\n";
	    }
	    stream << "</result>\r\n";
	    stream << flush;
	    if(data_list)
		delete data_list;
	    atomic_add_long(&statistic.http_requests, 1);
	}
    }

    s_->close();
    delete s_;
}
