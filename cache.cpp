//
// Created by henrylee on 19-3-28.
//

#include "cache.h"

using namespace std;

shared_ptr<char> CacheLFU::get(const RefString& path) {
  shared_ptr<char> result = nullptr;
  lock_guard<mutex> lock(cache_mutex);

  // search cache
  for (int i = 0; i < cache_size; i++) {
    if (path == keys[i]) {
      // found. so add its frequency counter
      result = vals[i];
      frequencies[i]++;
      break;
    }
  }

  return result;
}

bool CacheLFU::set(const RefString& path, shared_ptr<char>& data) {
  lock_guard<mutex> lock(cache_mutex);

  int min_index = 0;
  auto min_freq = frequencies[min_index];
  for (int i = 0; i < cache_size; i++) {
    // this cache line not used
    if (!frequencies[i]) {
      min_index = i;
      break;
    }

    // find min frequency
    if (min_freq > frequencies[i]) {
      min_index = i;
      min_freq = frequencies[min_index];
    }
  }

  // free it
  if (vals[min_index].get()) {
    delete vals[min_index].get();
  }
  // replace it
  keys[min_index] = path;
  vals[min_index] = data;
  frequencies[min_index] = 1;

  return true;
}

shared_ptr<char> CacheLFU::operator[](const RefString& path) {
  return get(path);
}

shared_ptr<char> CacheLRU::get(const RefString &path) {
  shared_ptr<char> result = nullptr;

  lock_guard<mutex> lock(cache_mutex);

  // find key in cache
  for (int i = 0; i < cache_size; i++) {
    if (path == keys[i]) {
      result = vals[i];
      recent_flags[i] = 0;
      break;
    }
  }

  for (int i = 0; i < cache_size; i++) {
    if (recent_flags[i] < 0xffffffff) {
      // set this cache line time flag to latest
      recent_flags[i]++;
    }
  }

  return result;
}

bool CacheLRU::set(const RefString &path, shared_ptr<char>& data) {
  lock_guard<mutex> lock(cache_mutex);

  // find oldest cache line
  int oldest_index = 0;
  uint32_t oldest_time = recent_flags[oldest_index];
  for (int i = 0; i < cache_size; i++) {
    if (oldest_time < recent_flags[i]) {
      oldest_index = i;
      oldest_time = recent_flags[oldest_index];
    }
  }

  // free it
  if (vals[oldest_index].get()) {
    delete vals[oldest_index].get();
  }
  // replace it
  keys[oldest_index] = path;
  vals[oldest_index] = data;
  recent_flags[oldest_index] = 0;

  // all cache lines time flows
  for (int i = 0; i < cache_size; i++) {
    if (recent_flags[i] < 0xffffffff) {
      recent_flags[i]++;
    }
  }
  return true;
}

shared_ptr<char> CacheLRU::operator[](const RefString &path) {
  return get(path);
}
