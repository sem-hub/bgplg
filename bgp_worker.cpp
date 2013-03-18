#include <sys/types.h>
#include <netinet/in.h>
#include <machine/atomic.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <log4cpp/Category.hh>
#include "global.hpp"
#include "socket.hpp"
#include "bgp_worker.hpp"
#include "bgp.h"
#include "bgp_attributes.hpp"
#include "object_collection.hpp"
#include "rwlock.hpp"
#include "ip_addr.hpp"

using namespace std;

struct peer_equal : public binary_function<pair<ip_addr, bgp_worker*>, ip_addr, bool> {
    bool operator()(pair<ip_addr, bgp_worker*> x, ip_addr y) const {
	return x.first.as_in_addr() == y.as_in_addr();
    }
};

void
bgp_worker::Execute(void *arg)
{
    struct msg_header header;
    int i;
    bool bad;
    bgp_worker *found=NULL;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    s_ = reinterpret_cast<Socket*>(arg);
    peer_.set_addr(s_->get_peer(), HOST_ORDER);
    slog.infoStream() << "BGP connected from: " << peer_.as_string() << eol;

    {
	/* accure a write lock for this block */
	rwlock rlock(RWLOCK, peers_lock);

	/* check for double connection */
	for(list<pair<ip_addr, bgp_worker*> >::const_iterator ii=peers.begin();
		ii != peers.end(); ++ii)
	    if(peer_ == (*ii).first) {
		slog.noticeStream() << "The peer (" << peer_.as_string() << ") already connected. Closing stale connection." << eol;
		found = (*ii).second;
		break;
	    }
    }
    if(found) {
	/* We want to be sure, we have no objects for this peer */
	objs.remove_all_for_peer(peer_);
	peers.remove_if(bind2nd(peer_equal(), peer_));
	found->stop();
    } else {
	atomic_add_long(&statistic.peers_number, 1);
	peers.push_back(pair<ip_addr, bgp_worker*>(peer_, this));
    }

    while(!stop_) {
	if(hold_time > 0 && time(NULL)-last_keepalive >= hold_time-1)
	    send_keepalive();

	try {
	    s_->read_assure((char*)&header, sizeof(header));
	} catch(SocketNotEnoughData) {
	    slog.log(log4cpp::Priority::ERROR, "Not enough data from the peer");
	    break;
	} catch(SocketOsError) {
	    slog.log(log4cpp::Priority::ERROR, "Socket error");
	    break;
	}

	bad = false;
	for(i=0; i<MSGSIZE_HEADER_MARKER; ++i) {
	    if(header.marker[i] != 0xff) {
		bad = true;
		slog.log(log4cpp::Priority::ERROR, "Bad header marker");
		break;
	    }
	}
	/* Drop the connection on error */
	if(bad)
	    break;

	try {
	switch(header.type) {
	    case OPEN:
		slog.log(log4cpp::Priority::DEBUG, "Got OPEN message");
		process_open(header);
		break;
	    case UPDATE:
		slog.log(log4cpp::Priority::DEBUG, "Got UPDATE message");
		process_update(header);
		break;
	    case NOTIFICATION:
		slog.log(log4cpp::Priority::DEBUG, "Got NOTIFICATION message");
		process_notification(header);
		break;
	    case KEEPALIVE:
		slog.log(log4cpp::Priority::DEBUG, "Got KEEPALIVE message");
		send_keepalive();
		break;
	    case RREFRESH:
		slog.log(log4cpp::Priority::DEBUG, "Got RREFRESH message");
		break;
	    default:
		slog.noticeStream() << "Got unknown message: " << header.type << eol;
	}
	} catch(BadData) {
	    slog.log(log4cpp::Priority::ERROR, "Got wrong data from the peer");
	    break;
	} catch(SocketOsError) {
	    slog.log(log4cpp::Priority::ERROR, "Socket error");
	    break;
	}
    }

    slog.infoStream() << "close connection with " << peer_.as_string() << eol;

    /* If it was a force stopping, we already free all data */
    if(!stop_) {
	objs.remove_all_for_peer(peer_);

	{
	    rwlock rlock(RWLOCK, peers_lock);
	    peers.remove_if(bind2nd(peer_equal(), peer_));
	}
	atomic_subtract_long(&statistic.peers_number, 1);
    }

    s_->close();
    delete s_;
}

void
bgp_worker::process_open(const struct msg_header& header)
{
    char *buf;
    struct msg_header *h;
    struct msg_open msg_o, *o;
    size_t buflen;
    ip_addr bgpid;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    try {
	s_->read_assure((char*)&msg_o, sizeof(msg_o));
    } catch (SocketNotEnoughData) {
	throw BadData();
    }
    bgpid.set_addr(msg_o.bgpid);
    slog.debugStream() << "Version: " << (int)msg_o.version << ",\tAS" << ntohs(msg_o.myas) << eol;
    slog.debugStream() << "Hold time: " << ntohs(msg_o.holdtime) << ",\tBDPID: " << bgpid.as_string() << eol;
    slog.debugStream() << "Param len: " << (int)msg_o.optparamlen << eol;

    hold_time = ntohs(msg_o.holdtime);

    /* Read rest of OPEN message (opt params) */
    buf = (char*)malloc(msg_o.optparamlen);
    if(buf == NULL)
	throw BadAlloc();
    try {
	s_->read_assure(buf, msg_o.optparamlen);
    } catch(SocketNotEnoughData) {
	throw BadData();
    }
    free(buf);

    slog.log(log4cpp::Priority::DEBUG, "Send OPEN");
    buflen = sizeof(struct msg_header)+sizeof(struct msg_open);
    buf = (char*)malloc(buflen);
    if(buf == NULL)
	throw BadAlloc();
    h = (struct msg_header*)buf;
    memset(h->marker, 0xff, MSGSIZE_HEADER_MARKER);
    h->len = htons(buflen);
    h->type = OPEN;
    o = (struct msg_open*)(buf+sizeof(struct msg_header));
    o->version = 4;
    o->myas = htons(ouras);
    o->bgpid = bindip.as_in_addr();
    o->optparamlen = 0;

    s_->write(buf, buflen);
    free(buf);
}

void
bgp_worker::process_update(const struct msg_header& header)
{
    char *buf;
    uint16_t withdraw, attrs;
    size_t buflen, n;
    int i;
    bgp_prefix prfx;
    bgp_attributes *bgp_attrs;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");
    //log4cpp::Category &jlog = log4cpp::Category::getInstance("bgplg-journal");

    if(ntohs(header.len) < sizeof(struct msg_header))
	throw BadData();
    buflen = ntohs(header.len)-sizeof(struct msg_header);
    buf = (char*)malloc(buflen);
    if(buf == NULL)
	throw BadAlloc();

    try {
	s_->read_assure(buf, buflen);
    } catch(SocketNotEnoughData) {
	throw BadData();
    }

    /* Withdraw routes */
    withdraw = ntohs(*(uint16_t*)buf);
    if(withdraw > buflen-2/*uint16_t*/)
	throw BadData();
    n=sizeof(uint16_t); i=1;
    while(n<withdraw+sizeof(uint16_t)) {
	try {
	    prfx.parse((u_char*)buf+n);
	} catch (invalid_argument) {
	    throw BadData();
	}
	slog.debugStream() << "withdraw " << prfx.as_string() << eol;
	/*jlog.infoStream() << "peer: " << peer_.as_string() << " withdraw " << prfx.as_string() << eol; */
	objs.remove(prfx, peer_);
	atomic_add_long(&statistic.withdraw_processed, 1);

	n += ceil(prfx.get_len()/8.)+1;
	++i;

	if(!dont_write_sql) {
	    stringstream query;
	    query << "INSERT INTO updates (prefix, next_hop, peer) VALUES ('" << prfx.as_string() << "', 'WITHDRAW', '" << peer_.as_string() << "')";
	    mysql_real_query(mysql, query.str().c_str(), query.str().length());
	}
    }

    /* BGP attributes */
    attrs = ntohs(*(uint16_t*)(buf+n));
    /* There are no attributes */
    if(attrs == 0)
	return;
    
    n += sizeof(uint16_t); /* attrs len */
    bgp_attrs = new bgp_attributes(peer_, buf+n, attrs);
    n += attrs;
    /* Clone the attributes for each prefix */
    i = 1;
    while(n<buflen) {
	if(i > 1)
	    bgp_attrs = bgp_attrs->clone();
	bgp_attrs->set_prefix((u_char*)buf+n);
	slog.debugStream() << "UPDATE prefix: " << bgp_attrs->get_prefix() << eol;
	/*jlog.infoStream() << "prefix: " << bgp_attrs->get_prefix() 
	    << " data:\r\n" << bgp_attrs->output_xml() << eol;*/
	objs.add_or_replace(bgp_attrs, peer_);
	n += ceil(buf[n]/8.)+1;
	++i;
	if(!dont_write_sql) {
	    string query = bgp_attrs->as_sql();
	    mysql_real_query(mysql, query.c_str(), query.length());
	}
    }
    free(buf);
    atomic_add_long(&statistic.updates_processed, 1);
}

void
bgp_worker::process_notification(const struct msg_header& header)
{
    char *buf;
    u_int code, subcode;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    if(ntohs(header.len) < sizeof(struct msg_header))
	throw BadData();
    buf = (char*)malloc(ntohs(header.len)-sizeof(struct msg_header));
    if(buf == NULL)
	throw BadAlloc();
    try {
	s_->read_assure(buf, ntohs(header.len)-sizeof(struct msg_header));
    } catch (SocketNotEnoughData) {
	free(buf);
	throw BadData();
    }
    
    code = buf[0];
    subcode = buf[1];
    slog.debugStream() << "Error code: " << code << " subcode: " << subcode << eol;
    free(buf);
    if(code == 4)
	stop();
}

void
bgp_worker::send_keepalive()
{
    struct msg_header header;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    slog.log(log4cpp::Priority::DEBUG, "Send KEEPALIVE message");
    memset(header.marker, 0xff, MSGSIZE_HEADER_MARKER);
    header.len = htons(sizeof(struct msg_header));
    header.type = KEEPALIVE;
    s_->write((char*)&header, sizeof(header));

    last_keepalive = time(NULL);
}
