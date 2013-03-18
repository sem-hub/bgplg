#include "strings.h"
#include <list>
#include <stdexcept>
#include <log4cpp/Category.hh>
#include "global.hpp"
#include "object_collection.hpp"
#include "bgp_attributes.hpp"
#include "rt.hpp"

using namespace std;

object_collection::object_collection() : head(NULL)
{ 
    rwlock::init(collection_lock); 
    bzero((void*)hash_table, sizeof(std::list<bgp_attributes*>*)*HASH_SIZE);
}

// ip/mask, peer-ip
#define HASH_KEY_LEN (sizeof(in_addr_t)+sizeof(uint8_t)+sizeof(in_addr_t))

void
object_collection::add_or_replace(bgp_attributes *attrs, const ip_addr& peer)
{
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    attrs->peer = peer;
    slog.debugStream() << "add object " << attrs->prfx.as_string() << " peer: " << peer.as_string() << eol;

    /* Remove an old entry if exists */
    remove(attrs->prfx, peer);

    rwlock rlock(RWLOCK, collection_lock);

    hash_add(attrs, attrs->prfx.get_net().as_in_addr(), attrs->prfx.get_len(), peer.as_in_addr());
    rt.add(attrs, attrs->prfx.get_net().as_in_addr(), attrs->prfx.get_len());
    if(head == NULL)
	head = attrs;
    else {
	/* Inser into front */
	attrs->next = head;
	head->previous = attrs;
	head = attrs;
    }
    statistic.prefixes_number++;
}

list<bgp_attributes*>*
object_collection::get_nearest(const in_addr_t net, const uint8_t mask)
{
    list<bgp_attributes*> *l, *new_list;
    bgp_attributes *attrs;

    rwlock rlock(RLOCK, collection_lock);
    l = rt.find(net, mask);
    /* Not found */
    if(l == NULL)
	return NULL;
    new_list = new list<bgp_attributes*>;
    for(list<bgp_attributes*>::const_iterator ii=l->begin();
	    ii != l->end(); ++ii) {
	attrs = (*ii)->clone();
	new_list->push_back(attrs);
    }

    return new_list;
}

void
object_collection::remove(bgp_prefix& prfx, const ip_addr peer)
{
    bgp_attributes* attrs;
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    rwlock rlock(RWLOCK, collection_lock);

    in_addr_t net = prfx.get_net().as_in_addr();
    u_char mask = prfx.get_len();

    attrs = hash_lookup(net, mask, peer.as_in_addr());
    if(attrs != NULL) {
	slog.debugStream() << "remove object " << prfx.as_string() << " peer: " << peer.as_string() << eol;
	rt.remove(net, mask, peer.as_in_addr());
	hash_remove(net, mask, peer.as_in_addr());
	if(attrs->next)
	    attrs->next->previous = attrs->previous;
        if(attrs == head)
            head = attrs->next;
        else
            attrs->previous->next = attrs->next;
	delete attrs;
	statistic.prefixes_number--;
    } else {
	slog.debugStream() << "remove object " << prfx.as_string() << " peer: " << peer.as_string() << ": not found" << eol;
    }
}

void
object_collection::remove_all_for_peer(const ip_addr peer)
{
    bgp_attributes *ii, *tmp;
    log4cpp::Category &jlog = log4cpp::Category::getInstance("bgplg-journal");

    rwlock rlock(RWLOCK, collection_lock);
    for(ii = head; ii != NULL;) {
	if(ii->peer == peer) {
	    if(ii == head) {
		head = ii->next;
		head->previous = NULL;
		tmp = head;
	    } else {
		ii->previous->next = ii->next;
		if(ii->next)
		    ii->next->previous = ii->previous;
		tmp = ii->next;
	    }
	    jlog.infoStream() << "peer: " << peer.as_string() << " remove prefix: " << ii->prfx.as_string() << eol;
	    hash_remove(ii->prfx.get_net().as_in_addr(), ii->prfx.get_len(), peer.as_in_addr());
	    rt.remove(ii->prfx.get_net().as_in_addr(), ii->prfx.get_len(), ii->peer.as_in_addr());
	    delete ii;
	    statistic.prefixes_number--;
	    ii = tmp;
	} else
	    ii = ii->next;
    }
}

uint32_t
object_collection::sdbm_hash(const uint8_t *key, const size_t len)
{
    uint32_t hash = 0;

    for (size_t i = 0; i < len; i++)
        hash = key[i] + (hash << 6) + (hash << 16) - hash;

    return hash % HASH_SIZE;
}

size_t
object_collection::make_hash_key(uint8_t *buf, const in_addr_t net, const uint8_t mask, const in_addr_t peer)
{
    memcpy((void*)buf, (void*)&net, sizeof(in_addr_t));
    memcpy((void*)(buf+sizeof(in_addr_t)), (void*)&mask, sizeof(uint8_t));
    memcpy((void*)(buf+sizeof(in_addr_t)+sizeof(uint8_t)), (void*)&peer, sizeof(in_addr_t));

    return HASH_KEY_LEN;
}

bgp_attributes *
object_collection::hash_lookup(in_addr_t net, uint8_t mask, in_addr_t peer)
{
    list<bgp_attributes*> *ptr;
    size_t len;
    uint8_t buf[HASH_KEY_LEN];

    len = make_hash_key(buf, net, mask, peer);

    ptr = hash_table[sdbm_hash(buf, len)];
    if(ptr == NULL || ptr->empty())
	return NULL;

    for(list<bgp_attributes*>::const_iterator ii = ptr->begin(); ii != ptr->end(); ++ii)
	if((*ii)->peer.as_in_addr() == peer &&
		(*ii)->prfx.get_net().as_in_addr() == net &&
		(*ii)->prfx.get_len() == mask)
	    return *ii;

    return NULL;
}

void
object_collection::hash_add(bgp_attributes* val, const in_addr_t net, const uint8_t mask, const in_addr_t peer)
{
    list<bgp_attributes*> *ptr;
    size_t len;
    uint8_t buf[HASH_KEY_LEN];
    log4cpp::Category &slog = log4cpp::Category::getInstance("bgplg");

    len = make_hash_key(buf, net, mask, peer);
    ptr = hash_table[sdbm_hash(buf, len)];
    if(ptr == NULL)
	ptr = hash_table[sdbm_hash(buf, len)] = new list<bgp_attributes*>;

    ptr->push_back(val);
    slog.debugStream() << "hash backets: " << ptr->size() << eol;
}

void
object_collection::hash_remove(const in_addr_t net, const uint8_t mask, const in_addr_t peer)
{
    list<bgp_attributes*> *ptr;
    size_t len;
    uint8_t buf[HASH_KEY_LEN];

    len = make_hash_key(buf, net, mask, peer);

    ptr = hash_table[sdbm_hash(buf, len)];
    if(ptr == NULL || ptr->empty())
	return;

    for(list<bgp_attributes*>::iterator ii = ptr->begin(); ii != ptr->end(); ++ii)
	if((*ii)->peer.as_in_addr() == peer &&
		(*ii)->prfx.get_net().as_in_addr() == net &&
		(*ii)->prfx.get_len() == mask) {
	    ptr->erase(ii);
	    break;
	}
}
