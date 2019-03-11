CC = gcc
CFLAGS = -Wall -Og -g -D_REENTRANT
LDFLAGS = -lpthread -pthread

OBJS = cache.o thread_pool.o queue.o util.o logger.o

web_server: $(OBJS) server.o
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

queue.o: queue.c queue.h
logger.o: logger.c logger.h
server.o: server.c 
thread_pool.o: thread_pool.c thread_pool.h
cache.o: cache.c cache.h

clean:
	rm web_server *.o
