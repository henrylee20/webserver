# Simple Web Server
一个简单的静态Web服务器,熟悉一下线程池

### Compile:
    make clean && make

### Run:
    ./web_server <端口> <运行目录> <dispatch线程数量> <worker线程数量> <是否动态调整worker (参数取值0或1)> <request队列长度> <cache大小>

程序主要包括以下几个部分:
    
* 线程池: thread_pool.c thread_pool.h
* 缓存: cache.c cache.h
* 日志: logger.c logger.h
* 队列: queue.c queue.h