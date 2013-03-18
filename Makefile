PROG=	bgplg
OBJS=	main.o thread.o socket.o bgp_attributes.o bgp_worker.o http_worker.o object_collection.o rt.o ip_addr.o bgp_prefix.o config.o

CFLAGS=		-I/usr/local/include -I/usr/local/include/mysql -g -Wall
LDFLAGS=	-L/usr/local/lib -L/usr/local/lib/mysql -pthread -lutil -llog4cpp -lmysqlclient

all:	${PROG} ${OBJS}

${PROG}: ${OBJS}
	${CXX} -o ${.TARGET} ${LDFLAGS} $>

clean:
	rm -f ${PROG} *.o *.bak *.core *.gch .depend

depend:
	${CXX} -E -MM *.cpp > .depend
