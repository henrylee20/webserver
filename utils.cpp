//
// Created by henrylee on 19-4-1.
//

#include <unistd.h>
#include <errno.h>
#include <iostream>
#include "utils.h"

FileLock::FileLock(const char *filename) {
  fd = open(filename, O_RDWR | O_CREAT | O_TRUNC);
  unlink(filename);

  lock_it.l_type = F_WRLCK;
  lock_it.l_whence = SEEK_SET;
  lock_it.l_start = 0;
  lock_it.l_len = 0;

  unlock_it.l_type = F_UNLCK;
  unlock_it.l_whence = SEEK_SET;
  unlock_it.l_start = 0;
  unlock_it.l_len = 0;

}

FileLock::~FileLock() {
  //close(fd);
}

void FileLock::lock() {
  int rc;
  while ((rc = fcntl(fd, F_SETLKW, &lock_it)) < 0) {
    if (errno == EINTR) {
      continue;
    } else {
      std::cerr << "fcntl err: " << errno << std::endl;
      exit(errno);
    }
  }
}

void FileLock::unlock() {
  if (fcntl(fd, F_SETLKW, &unlock_it) < 0)
    std::cerr << "fcntl err: " << errno << std::endl;
}
