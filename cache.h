//
// Created by henrylee on 19-3-28.
//

#ifndef HTTP_SERVER_CACHE_H
#define HTTP_SERVER_CACHE_H

#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include "ref_string.h"

class Cache {
protected:
  uint64_t cache_size;
  std::vector<RefString> keys;
  std::vector<std::shared_ptr<char>> vals;
  std::mutex cache_mutex;

protected:
  explicit Cache(uint64_t cache_size) : cache_size(cache_size), keys(cache_size, ""), vals(cache_size, nullptr) {}

public:
  virtual std::shared_ptr<char> get(const RefString& path) = 0;
  virtual bool set(const RefString& path, std::shared_ptr<char>& data) = 0;
  virtual std::shared_ptr<char> operator[](const RefString& path) = 0;

  Cache(const Cache& cache) = delete;
  Cache(Cache&& cache) = delete;
  Cache& operator=(Cache&) = delete;
};

class CacheLFU : public Cache {
private:
  std::vector<uint32_t> frequencies;

public:
  explicit CacheLFU(uint64_t cache_size) : Cache(cache_size), frequencies(cache_size, 0) {}

  std::shared_ptr<char> get(const RefString& path) final;
  bool set(const RefString& path, std::shared_ptr<char>& data) final;
  std::shared_ptr<char> operator[](const RefString& path) final;
};

class CacheLRU : public Cache {
private:
  std::vector<uint32_t> recent_flags;

public:
  explicit CacheLRU(uint64_t cache_size) : Cache(cache_size), recent_flags(cache_size, 0xffffffff) {}

  std::shared_ptr<char> get(const RefString& path) final;
  bool set(const RefString& path, std::shared_ptr<char>& data) final;
  std::shared_ptr<char> operator[](const RefString& path) final;
};


#endif //HTTP_SERVER_CACHE_H
