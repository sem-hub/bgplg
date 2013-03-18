#include <stdlib.h>
#include <list>
#include <stdexcept>
#include "bgp_attributes.hpp"
#include "rt.hpp"

using namespace std;

RadixTree::RadixTree()
{
    rt_root = new struct rt_node;
    rt_root->parent = NULL;
    rt_root->right = NULL;
    rt_root->left = NULL;
}

list<bgp_attributes*>*
RadixTree::find(in_addr_t net, uint8_t mask)
{
    uint32_t bit = 0x80000000;
    struct rt_node *result = NULL, *node = rt_root;

    while(node) {
	if(!node->data_list.empty())
	    result = node;

	if(net & bit)
	    node = node->right;
	else
	    node = node->left;

	bit >>= 1;
    }

    if(result == NULL)
	return NULL;
    return &result->data_list;
}

int
RadixTree::add(bgp_attributes *attrs, in_addr_t net, uint8_t mask)
{
    uint32_t bit = 0x80000000;
    struct rt_node *node, *next;

    node = next = rt_root;

    while (bit & (0xffffffff << (32-mask))) {
	if(net & bit)
	    next = node->right;
	else
	    next = node->left;
	if(next == NULL)
	    break;
	bit >>= 1;
	node = next;
    }

    /* We already have an allocated entry. Just fill it. */
    if(next != NULL) {
	node->data_list.push_back(attrs);
	return 1;
    }

    /* Create an entry and all entries till it. */
    while(bit & (0xffffffff << (32-mask))) {
	next = new struct rt_node;

	next->right = NULL;
	next->left = NULL;
	next->parent = node;

	if(net & bit)
	    node->right = next;
	else
	    node->left = next;

	bit >>= 1;
	node = next;
    }

    node->data_list.push_back(attrs);

    return 1;
}

int
RadixTree::remove(in_addr_t net, uint8_t mask, in_addr_t peer)
{
    uint32_t bit = 0x80000000;
    struct rt_node *node = rt_root, *tmp;

    while (node && (bit & (0xffffffff << (32-mask)))) {
	if(net & bit)
	    node = node->right;
	else
	    node = node->left;
	bit >>= 1;
    }

    /* Not found */
    if(node == NULL)
	return 0;

    for(list<bgp_attributes*>::iterator ii=node->data_list.begin();
	    ii != node->data_list.end(); ++ii)
	if(peer == (*ii)->peer.as_in_addr()) {
	    node->data_list.erase(ii);
	    break;
	}
    if(!node->data_list.empty())
	/* We still have entries in the list. Keep the node. */
	return 1;

    /* We have both branches. Just clear data. */
    if(node->right || node->left)
	return 1;

    while(1) {
	if(node->parent->right == node)
	    node->parent->right = NULL;
	else
	    node->parent->left = NULL;

	tmp = node;
	node = node->parent;
	free(tmp);

	if(node->right || node->left)
	    break;
	if(!node->data_list.empty())
	    break;
	if(node->parent == NULL)
	    break;
    }

    return 1;
}
