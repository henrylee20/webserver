//
// Created by henrylee on 19-4-1.
//

#ifndef HTTP_SERVER_UTILS_H
#define HTTP_SERVER_UTILS_H

#include <fcntl.h>

class FileLock {
private:
  int fd;
  flock lock_it{};
  flock unlock_it{};

public:
  explicit FileLock(const char* filename);
  ~FileLock();
  void lock();
  void unlock();
};

#endif //HTTP_SERVER_UTILS_H
