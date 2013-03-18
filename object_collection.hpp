#ifndef _OBJECT_COLLECTION_H
#define _OBJECT_COLLECTION_H
#include <sys/types.h>
#include <list>
#include "rwlock.hpp"
#include "bgp_prefix.hpp"

const uint32_t HASH_SIZE=128000;
class bgp_attributes;

class object_collection {
    public:
	object_collection();
	void add_or_replace(bgp_attributes *, const ip_addr& peer);
	std::list<bgp_attributes*> *get_nearest(in_addr_t net, uint8_t mask);
	void remove(bgp_prefix& prfx, ip_addr peer);
	void remove_all_for_peer(ip_addr peer);
    private:
	bgp_attributes	*head;
	std::list<bgp_attributes*> *hash_table[HASH_SIZE];
	pthread_rwlock_t collection_lock;

	uint32_t sdbm_hash(const uint8_t *key, size_t len);
	bgp_attributes *hash_lookup(in_addr_t net, uint8_t mask, in_addr_t peer);
	size_t make_hash_key(uint8_t *, in_addr_t, uint8_t, in_addr_t);
	void hash_add(bgp_attributes *, in_addr_t, uint8_t, in_addr_t);
	void hash_remove(in_addr_t, uint8_t, in_addr_t);
};

#endif
