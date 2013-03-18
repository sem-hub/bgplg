#ifndef _RT_H
#define _RT_H
#include <sys/types.h>
#include <list>

class bgp_attributes;

struct rt_node {
    std::list<bgp_attributes*> data_list;

    struct rt_node* parent;
    struct rt_node* left;
    struct rt_node* right;
};

class RadixTree {
    private:
	struct rt_node	*rt_root;
    public:
	RadixTree();
	std::list<bgp_attributes*>* find(in_addr_t net, uint8_t mask);
	int add(bgp_attributes *attrs, in_addr_t net, uint8_t mask);
	int remove(in_addr_t net, uint8_t mask, in_addr_t peer);
};

#endif
