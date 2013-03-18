#include <sys/types.h>
#include <netinet/in.h>
#include <math.h>
#include <string>
#include <sstream>
#include <iostream>
#include <log4cpp/Category.hh>
#include "bgp.h"
#include "global.hpp"
#include "exceptions.hpp"
#include "bgp_attributes.hpp"
#include "ip_addr.hpp"

using namespace std;

bgp_attributes::bgp_attributes(ip_addr cpeer, const char *buf, uint16_t len) : peer(cpeer),
    prfx(0,0), origin(0), atomic_aggregate(false), next_hop(0), med(0), local_pref(0)
{
    uint16_t alen, n=0, i;
    uint8_t code, flags;
    ext_comm_t ext_comm;

    /* Initialize */
    aggregator.as = 0;
    aggregator.ip = 0;
    previous = next = NULL;

    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    while(n < len) {
	flags = buf[n++];
	code = buf[n++];
	/* alen is one or two bytes */
	if(flags & 0x10) {
	    alen = ntohs(*(uint16_t*)(buf+n));
	    n+=2;
	} else {
	    alen = (uint8_t)buf[n];
	    n+=1;
	}
	/* is alen make an overflow? */
	if(alen > len-n)
	    throw BadData();
	switch(code) {
	    case BGP_ORIGIN:
		origin = buf[n];
		break;
	    case BGP_ASPATH:
	    case BGP_AS4_ASPATH:
		/* buf[n] is AS_SET|AS_SEQUENCE|AS_CONFED_SEQUENCE|AS_CONFED_SET
		 * buf[n+1] - number of ASes
		 */
		for(i=0; i<(uint8_t)buf[n+1]; ++i)
		    if(code == BGP_ASPATH)
			aspath.push_back((uint32_t)ntohs(*(uint16_t*)(buf+n+2+i*sizeof(uint16_t))));
		    else
			aspath.push_back(ntohl(*(uint32_t*)(buf+n+2+i*sizeof(uint32_t))));
		break;
	    case BGP_NEXTHOP:
		next_hop.set_addr(*(in_addr_t*)(buf+n));
		break;
	    case BGP_MED:
		med = ntohl(*(uint32_t*)(buf+n));
		break;
	    case BGP_LOCALPREF:
		local_pref = ntohl(*(uint32_t*)(buf+n));
		break;
	    case BGP_ATOMICAGGREGATE:
		atomic_aggregate=true;
		break;
	    case BGP_AGGREGATOR:
	    case BGP_AS4_AGGREGATED:
		if(code == BGP_AGGREGATOR) {
		    aggregator.as = (int32_t)ntohs(*(uint16_t*)(buf+n));
		    aggregator.ip = ntohl(*(in_addr_t*)(buf+n+2));
		} else {
		    aggregator.as = ntohl(*(uint32_t*)(buf+n));
		    aggregator.ip = ntohl(*(in_addr_t*)(buf+n+4));
		}
		break;
	    case BGP_COMMUNITY:
		for(i=0; i<alen; i+=4)
		    communities.push_back(pair<uint16_t,uint16_t>(
				ntohs(*(uint16_t*)(buf+n+i)),
				ntohs(*(uint16_t*)(buf+n+sizeof(uint16_t)+i))
				));
		break;
	    case BGP_AS4_COMMUNITY:
		for(i=0; i<alen; i+=ECOMMUNITY_SIZE) {
		    /* buf[n+1] is Type of extended community:
		     * 0x02 - ROUTE TARGET
		     * 0x03 - SITE ORIGIN
		     */
		    switch(*(uint8_t*)(buf+n+i+1)) {
			case EXT_COMM_RT:
			    ext_comm.type = EXT_COMM_RT;
			    break;
			case EXT_COMM_SO:
			    ext_comm.type = EXT_COMM_SO;
			    break;
			default:
			    ext_comm.type = EXT_COMM_UNKN;
			    slog.noticeStream() << "Unknown extended community type: " << (int)*(uint8_t*)(buf+n+i+1) << eol;
		    }
		    /* buf[n]:
		     * 0x40 - transitive bit (just mask it)
		     * (2 - two bytes field, 4 - four bytes field)
		     * 0x00 - AS2:VAL4
		     * 0x01 - IP4:VAL2
		     * 0x02 - AS4:VAL2
		     */
		    switch(*(uint8_t*)(buf+n+i) & ~0x40) {
			case 0:
			    ext_comm.key = (uint32_t)ntohs(*(uint16_t*)(buf+n+i+2));
			    ext_comm.value = ntohl(*(uint32_t*)(buf+n+i+2+2));
			    break;
			case 1:
			    ext_comm.type |= EXT_COMM_IP;
			case 2:
			    ext_comm.key = ntohl(*(uint32_t*)(buf+n+i+2));
			    ext_comm.value = (uint32_t)ntohs(*(uint16_t*)(buf+n+i+2+4));
			    break;
			default:
			    slog.noticeStream() << "Unknown extended community code: " << (int)(*(uint8_t*)(buf+n+i) & ~0x40) << eol;
		    }
		    ext_communities.push_back(ext_comm);
		}
		break;
	    default:
		slog.noticeStream() << "process_update(): Unknown BGP code: " << (int)code << eol;
	}
	n += alen;
    }
}

bgp_attributes*
bgp_attributes::clone()
{
    bgp_attributes *nattrs = new bgp_attributes();

    nattrs->next = NULL;
    nattrs->previous = NULL;

    nattrs->peer = peer;
    nattrs->prfx = prfx;
    nattrs->origin = origin;
    nattrs->atomic_aggregate = atomic_aggregate;
    nattrs->next_hop = next_hop;
    nattrs->med = med;
    nattrs->local_pref = local_pref;
    nattrs->aggregator.as = aggregator.as;
    nattrs->aggregator.ip = aggregator.ip;

    /* copy vectors */
    nattrs->aspath = aspath;
    nattrs->communities = communities;
    nattrs->ext_communities = ext_communities;

    return nattrs;
}

void
bgp_attributes::set_prefix(const bgp_prefix& _prfx)
{
    prfx = _prfx;
}

string
bgp_attributes::get_prefix()
{
    return prfx.as_string();
}

string
bgp_attributes::output_xml()
{
    ostringstream ss;

    ss << "  <peer ip=\"" << peer.as_string() << "\">\r\n";
    ss << "    <origin>";
    switch(origin) {
	case 0:
	    ss << "IGP";
	    break;
	case 1:
	    ss << "EGP";
	    break;
	default:
	    ss << "incomplete";
    }
    ss << "</origin>\r\n";
    ss << "    <next_hop>" << next_hop.as_string() << "</next_hop>\r\n";
    if(med)
	ss << "    <med>" << med << "</med>\r\n";
    if(local_pref)
	ss << "    <local_pref>" << local_pref << "</local_pref>\r\n";

    ss << "    <aspath>\r\n";
    for(vector<uint32_t>::const_iterator ii = aspath.begin();
	    ii != aspath.end(); ++ii)
	ss << "      <as>" << *ii << "</as>\r\n";
    ss << "    </aspath>\r\n";

    if(aggregator.as)
	ss << "    <aggregated_by><as>" << aggregator.as << "</as><ip>" << aggregator.ip.as_string() << "</ip></aggregated_by>\r\n";

    if(atomic_aggregate)
	ss << "    <atomic_aggregate/>\r\n";

    if(!communities.empty())
	for(vector<pair<uint16_t,uint16_t> >::const_iterator ii = communities.begin();
		ii != communities.end(); ++ii)
	    ss << "    <community>" << (*ii).first << ":" << (*ii).second << "</community>\r\n";

    if(!ext_communities.empty())
	for(vector<ext_comm_t>::const_iterator ii = ext_communities.begin();
		ii != ext_communities.end(); ++ii) {
	    ss << "    <extended_community>";
	    switch((*ii).type & ~EXT_COMM_IP) {
		case EXT_COMM_RT:
		    ss << "RT:";
		    break;
		case EXT_COMM_SO:
		    ss << "SO:";
		    break;
		default:
		    ss << "Unknown:";
	    }
	    if(((*ii).type & EXT_COMM_IP) != 0) {
		ip_addr ip((*ii).key, HOST_ORDER);
		ss << ip.as_string();
	    } else
		ss << (*ii).key;
	    ss << ":" << (*ii).value << "</extended_community>\r\n";
	}
    ss << "  </peer>\r\n";
    ss << flush;

    return ss.str();
}

string
bgp_attributes::as_sql()
{
    ostringstream query;

    query << "INSERT INTO updates (prefix, next_hop, aspath, community, attributes, peer) VALUES (";
    query << "'" << prfx.as_string() << "',";
    query << "'" << next_hop.as_string() << "','";
    for(vector<uint32_t>::const_iterator ii = aspath.begin();
	    ii != aspath.end(); ++ii)
	query << *ii << " ";
    query << "',";

    if(!communities.empty()) {
	query << "'";
	for(vector<pair<uint16_t,uint16_t> >::const_iterator ii = communities.begin();
		ii != communities.end(); ++ii)
	    query << (*ii).first << ":" << (*ii).second << " ";
    } else
	query << "'";
    query << "',";


    query << "'origin: ";
    switch(origin) {
	case 0:
	    query << "IGP";
	    break;
	case 1:
	    query << "EGP";
	    break;
	default:
	    query << "incomplete";
    }
    query << ";";
    if(med)
	query << "med: " << med << ";";
    if(local_pref)
	query << "local_pref: " << local_pref << ";";

    if(aggregator.as)
	query << "aggregated_by: as: " << aggregator.as << " ip: " << aggregator.ip.as_string() << ";";

    if(atomic_aggregate)
	query << "atomic_aggregate ";

    if(!ext_communities.empty())
	for(vector<ext_comm_t>::const_iterator ii = ext_communities.begin();
		ii != ext_communities.end(); ++ii) {
	    query << "extended_community: ";
	    switch((*ii).type & ~EXT_COMM_IP) {
		case EXT_COMM_RT:
		    query << "RT:";
		    break;
		case EXT_COMM_SO:
		    query << "SO:";
		    break;
		default:
		    query << "Unknown:";
	    }
	    if(((*ii).type & EXT_COMM_IP) != 0) {
		ip_addr ip((*ii).key, HOST_ORDER);
		query << ip.as_string();
	    } else
		query << (*ii).key;
	    query << ":" << (*ii).value << " ";
	}

    query << "','";
    query << peer.as_string() << "')";
    query << flush;

    return query.str();
}
