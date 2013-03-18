#ifndef _GLOBAL_H
#define _GLOBAL_H
#include <pthread.h>
#include <sys/types.h>
#include <list>
#include <log4cpp/CategoryStream.hh>
#include <mysql.h>
#include "ip_addr.hpp"

class bgp_worker;

extern class object_collection objs;
extern class RadixTree rt;
extern std::list<std::pair<ip_addr, bgp_worker*> > peers;
extern pthread_rwlock_t peers_lock;
extern ip_addr bindip;
extern uint32_t ouras;
extern bool dont_write_sql;
extern MYSQL *mysql;
extern std::string mysqlhost, mysqluser, mysqlpass;

using log4cpp::eol;

extern struct statistic {
    unsigned long       peers_number;
    unsigned long       prefixes_number;
    unsigned long       updates_processed;
    unsigned long       withdraw_processed;
    unsigned long	http_requests;
    unsigned long       start_time;
} statistic;

#endif
