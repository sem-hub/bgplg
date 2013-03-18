#ifndef _BGP_ATTRIBUTES_H
#define _BGP_ATTRIBUTES_H
#include <sys/types.h>
#include <vector>
#include <string>
#include "ip_addr.hpp"
#include "bgp_prefix.hpp"

typedef struct {
	uint32_t	as;
	ip_addr		ip;
} aggregator_t;

#define EXT_COMM_IP  0x10
#define EXT_COMM_RT  0x02
#define EXT_COMM_SO  0x03
#define EXT_COMM_UNKN 0xef
typedef struct {
	uint8_t		type;
	uint32_t	key;
	uint32_t	value;
} ext_comm_t;

class bgp_attributes {
    friend class object_collection;
    friend class RadixTree;
    private:
	ip_addr		peer;

	bgp_prefix	prfx;

	uint8_t		origin;
	bool		atomic_aggregate;
	std::vector<uint32_t>	aspath;
	ip_addr		next_hop;
	uint32_t	med;
	uint32_t	local_pref;
	std::vector<std::pair<uint16_t,uint16_t> > communities;
	std::vector<ext_comm_t>		ext_communities;
	aggregator_t	aggregator;
    protected:
	// A double linked list
	bgp_attributes	*next;
	bgp_attributes  *previous;
    public:
	bgp_attributes() : next(NULL), previous(NULL) {}
	bgp_attributes(ip_addr peer, const char *buf, uint16_t len);
	void set_prefix(const bgp_prefix& prfx);
	std::string get_prefix();
	std::string output_xml();
	std::string as_sql();
	bgp_attributes* clone();
};
#endif
