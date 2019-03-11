//
// Created by henrylee on 18-12-9.
//

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include "logger.h"
#include "cache.h"

/* ************************************ Base Cache Code ********************************/

typedef struct cache_impl_base_s {
  // inherit interface
  cache self;

  // private members
  void (*free_func)(void* target);
  int (*cmp)(void* key1, void* key2);
  pthread_mutex_t cache_lock;

  void** key_arr;
  void** val_arr;
  unsigned int cache_size;

} cache_impl_base;

cache_impl_base* cache_init_base(cache_impl_base* base,
                                 unsigned int cache_size,
                                 void (*free_func)(void* target),
                                 int (*cmp)(void* key1, void* key2)) {
  // private members
  base->free_func = free_func;
  base->cmp = cmp;

  if (pthread_mutex_init(&base->cache_lock, NULL) < 0) {
    logger_error("cache lock init error\n");
  }

  base->key_arr = (void**)malloc(sizeof(void*) * cache_size);
  memset(base->key_arr, 0, sizeof(void*) * cache_size);

  base->val_arr = (void**)malloc(sizeof(void*) * cache_size);
  memset(base->key_arr, 0, sizeof(void*) * cache_size);

  base->cache_size = cache_size;

  return base;
}

void cache_destroy_base(cache_impl_base* base) {
  if (base->free_func) {
    for (int i = 0; i < base->cache_size; i++) {
      if (base->val_arr[i]) {
        base->free_func(base->val_arr[i]);
      }
    }
  }

  base->cache_size = 0;
  free(base->val_arr);
  free(base->key_arr);

  if (pthread_mutex_destroy(&base->cache_lock) < 0) {
    logger_error("cache lock destroy error\n");
  }

  base->cmp = NULL;
  base->free_func = NULL;
}

/* ************************************ LFU Cache Code ********************************/

typedef struct cache_lfu_impl_s{
  // inherit cache_impl_base
  cache_impl_base base;

  // private members
  int* frequencies;
} cache_lfu_impl;

// LFU Cache 的对应接口的实现函数
void cache_lfu_destroy(struct cache_s* self);
void* cache_lfu_get(struct cache_s* self, void* key);
void cache_lfu_push(struct cache_s* self, void* key, void* val);

cache* cache_lfu_init(unsigned int cache_size, void (*free_func)(void* target), int (*cmp)(void* key1, void* key2)) {
  cache_lfu_impl* self_impl = (cache_lfu_impl*)malloc(sizeof(cache_lfu_impl));

  // call upper init
  cache_init_base(&self_impl->base, cache_size, free_func, cmp);

  // public func
  self_impl->base.self.destroy = cache_lfu_destroy;
  self_impl->base.self.get = cache_lfu_get;
  self_impl->base.self.push = cache_lfu_push;

  // private members
  self_impl->frequencies = (int*)malloc(sizeof(int) * cache_size);
  memset(self_impl->frequencies , 0, sizeof(int) * cache_size);

  return (cache*)self_impl;
}

void cache_lfu_destroy(struct cache_s* self) {
  cache_lfu_impl* self_impl = (cache_lfu_impl*)self;

  free(self_impl->frequencies);

  cache_destroy_base(&self_impl->base);

  free(self_impl);
}

void* cache_lfu_get(struct cache_s* self, void* key) {
  cache_lfu_impl* self_impl = (cache_lfu_impl*)self;
  void* result = NULL;

  if (pthread_mutex_lock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to lock cache\n");
  }

  // search cache
  int cmp_result = 0;
  for (int i = 0; i < self_impl->base.cache_size; i++) {

    if (self_impl->base.cmp) {
      cmp_result = self_impl->base.cmp(key, self_impl->base.key_arr[i]);
    } else {
      cmp_result = key == self_impl->base.key_arr[i];
    }

    if (cmp_result) {
      // found. so add its frequency counter
      result = self_impl->base.val_arr[i];
      __sync_fetch_and_add(&(self_impl->frequencies[i]), 1);
      break;
    }
  }

  if (pthread_mutex_unlock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to unlock cache\n");
  }

  return result;
}

void cache_lfu_push(struct cache_s* self, void* key, void* val) {
  cache_lfu_impl* self_impl = (cache_lfu_impl*)self;

  if (pthread_mutex_lock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to lock cache\n");
  }

  int min_index = 0;
  int min_freq = self_impl->frequencies[min_index];
  for (int i = 0; i < self_impl->base.cache_size; i++) {
    // this cache line not used
    if (!self_impl->frequencies[i]) {
      min_index = i;
      break;
    }

    // find min frequency
    if (min_freq > self_impl->frequencies[i]) {
      min_index = i;
      min_freq = self_impl->frequencies[min_index];
    }
  }

  // free it
  if (self_impl->base.val_arr[min_index] && self_impl->base.free_func) {
    self_impl->base.free_func(self_impl->base.val_arr[min_index]);
  }
  // replace it
  self_impl->base.key_arr[min_index] = key;
  self_impl->base.val_arr[min_index] = val;
  self_impl->frequencies[min_index] = 1;

  if (pthread_mutex_unlock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to unlock cache\n");
  }
}

/* ************************************ LRU Cache Code ********************************/

typedef struct cache_lru_impl_s{
  // inherit cache_impl_base
  cache_impl_base base;

  // private members
  unsigned int* recent_flags;
} cache_lru_impl;

// LRU Cache 的对应接口的实现函数
void cache_lru_destroy(struct cache_s* self);
void* cache_lru_get(struct cache_s* self, void* key);
void cache_lru_push(struct cache_s* self, void* key, void* val);

cache* cache_lru_init(unsigned int cache_size, void (*free_func)(void* target), int (*cmp)(void* key1, void* key2)) {
  cache_lru_impl* self_impl = (cache_lru_impl*)malloc(sizeof(cache_lru_impl));

  // call upper init
  cache_init_base(&self_impl->base, cache_size, free_func, cmp);

  // public func
  self_impl->base.self.destroy = cache_lru_destroy;
  self_impl->base.self.get = cache_lru_get;
  self_impl->base.self.push = cache_lru_push;

  // private members
  self_impl->recent_flags = (unsigned int*)malloc(sizeof(unsigned int) * cache_size);
  memset(self_impl->recent_flags , 0xff, sizeof(unsigned int) * cache_size);

  return (cache*)self_impl;
}

void cache_lru_destroy(struct cache_s* self) {
  cache_lru_impl* self_impl = (cache_lru_impl*)self;

  free(self_impl->recent_flags);

  cache_destroy_base(&self_impl->base);

  free(self_impl);
}

void* cache_lru_get(struct cache_s* self, void* key) {
  cache_lru_impl* self_impl = (cache_lru_impl*)self;
  void* result = NULL;

  if (pthread_mutex_lock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to lock cache\n");
  }

  // find key in cache
  int cmp_result = 0;
  for (int i = 0; i < self_impl->base.cache_size; i++) {
    if (self_impl->base.cmp) {
      cmp_result = self_impl->base.cmp(key, self_impl->base.key_arr[i]);
    } else {
      cmp_result = key == self_impl->base.key_arr[i];
    }

    if (cmp_result) {
      result = self_impl->base.val_arr[i];
      self_impl->recent_flags[i] = 0;
      break;
    }
  }

  for (int i = 0; i < self_impl->base.cache_size; i++) {
    if (self_impl->recent_flags[i] < 0xffffffff) {
      // set this cache line time flag to latest
      __sync_fetch_and_add(&(self_impl->recent_flags[i]), 1);
    }
  }

  if (pthread_mutex_unlock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to unlock cache\n");
  }

  return result;
}

void cache_lru_push(struct cache_s* self, void* key, void* val) {
  cache_lru_impl* self_impl = (cache_lru_impl*)self;

  if (pthread_mutex_lock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to lock cache\n");
  }

  // find oldest cache line
  int oldest_index = 0;
  int oldest_time = self_impl->recent_flags[oldest_index];
  for (int i = 0; i < self_impl->base.cache_size; i++) {
    if (oldest_time < self_impl->recent_flags[i]) {
      oldest_index = i;
      oldest_time = self_impl->recent_flags[oldest_index];
    }
  }

  // free it
  if (self_impl->base.val_arr[oldest_index] && self_impl->base.free_func) {
    self_impl->base.free_func(self_impl->base.val_arr[oldest_index]);
  }
  // replace it
  self_impl->base.key_arr[oldest_index] = key;
  self_impl->base.val_arr[oldest_index] = val;
  self_impl->recent_flags[oldest_index] = 0;

  // all cache lines time flows
  for (int i = 0; i < self_impl->base.cache_size; i++) {
    if (self_impl->recent_flags[i] < 0xffffffff) {
      __sync_fetch_and_add(&(self_impl->recent_flags[i]), 1);
    }
  }

  if (pthread_mutex_unlock(&self_impl->base.cache_lock) < 0) {
    logger_error("failed to unlock cache\n");
  }
}
