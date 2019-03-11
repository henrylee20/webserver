//
// Created by henrylee on 18-12-9.
//

#ifndef WEBSERVER_CACHE_H
#define WEBSERVER_CACHE_H

typedef struct cache_s{
  // 析构函数
  void (*destroy)(struct cache_s* self);

  // 根据索引key从cache中查找数据,找到了就返回当初存进来的指针,没找到返回NULL
  void* (*get)(struct cache_s* self, void* key);
  // 将数据val存入cache中,索引为key
  void (*push)(struct cache_s* self, void* key, void* val);
} cache;

// LFU cache 构造函数. free_func为释放数据内存的函数,cmp为比较两个索引key的函数
cache* cache_lfu_init(unsigned int cache_size, void (*free_func)(void* target), int (*cmp)(void* key1, void* key2));
// LRU cache 构造函数. free_func为释放数据内存的函数,cmp为比较两个索引key的函数
cache* cache_lru_init(unsigned int cache_size, void (*free_func)(void* target), int (*cmp)(void* key1, void* key2));

#endif //WEBSERVER_CACHE_H
