#ifndef _RWLOCK_HPP
#define _RWLOCK_HPP
#include <pthread.h>

enum lock_type { RLOCK, RWLOCK };

class rwlock {
    private:
	pthread_rwlock_t l;

	/* Disable the constructor */
	rwlock() {};
    public:
	rwlock(lock_type ltype, pthread_rwlock_t& lock) : l(lock) {
	    switch(ltype) {
		case RLOCK:
		    pthread_rwlock_rdlock(&l);
		    break;
		case RWLOCK:
		    pthread_rwlock_wrlock(&l);
	    }
	}
	~rwlock() { pthread_rwlock_unlock(&l); }
	static void init(pthread_rwlock_t& lock) { pthread_rwlock_init(&lock, 0); }
	static void destroy(pthread_rwlock_t& lock) { pthread_rwlock_destroy(&lock); }
};
#endif
