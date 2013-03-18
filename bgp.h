#ifndef _BGP_H
#define _BGP_H

#include <sys/types.h>

#define MSGSIZE_HEADER_MARKER 16
#define ECOMMUNITY_SIZE 8

enum msg_type {
        OPEN = 1,
        UPDATE,
        NOTIFICATION,
        KEEPALIVE,
        RREFRESH
};

enum update_type {
    	BGP_ORIGIN = 1,
	BGP_ASPATH,
	BGP_NEXTHOP,
	BGP_MED,
	BGP_LOCALPREF,
	BGP_ATOMICAGGREGATE,
	BGP_AGGREGATOR,
	BGP_COMMUNITY,
	BGP_AS4_COMMUNITY = 16,
	BGP_AS4_ASPATH,
	BGP_AS4_AGGREGATED
};

#pragma pack(1)
struct msg_header {
        u_char		marker[MSGSIZE_HEADER_MARKER];
        u_int16_t       len;
        u_int8_t        type;
};

struct msg_open {
        u_int8_t                 version;
        u_int16_t                myas;
        u_int16_t                holdtime;
        u_int32_t                bgpid;
        u_int8_t                 optparamlen;
};
#pragma pack()

#endif
