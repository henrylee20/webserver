//
// Created by henrylee on 19-4-1.
//

#ifndef HTTP_SERVER_HTTP_SERVICE_H
#define HTTP_SERVER_HTTP_SERVICE_H

#include <vector>
#include <unordered_set>
#include "http_listener.h"
#include "utils.h"

class HTTPSlave {
private:
  FileLock lock;
  int epoll_fd;
  int listener_fd;

  std::unordered_map<std::string, std::string> server_info = {
      {"Server", "Green Tea 0.1"},
  };

private:
  int addEvent(int fd, int events);
  int delEvent(int fd, int events);
  int modEvent(int fd, int events);

  int acceptHandler();
  int rdhupHandler(int fd);
  int requestHandler(int fd);

  std::string getGMTime();

  int errHandler(HTTPConnection& connection, HTTPRequest& request, int err_code);
  int getHandler(HTTPConnection& connection, HTTPRequest& request);
  int headHandler(HTTPConnection& connection, HTTPRequest& request);
  int postHandler(HTTPConnection& connection, HTTPRequest& request);
  int putHandler(HTTPConnection& connection, HTTPRequest& request);
  int delHandler(HTTPConnection& connection, HTTPRequest& request);
  int connectHandler(HTTPConnection& connection, HTTPRequest& request);
  int optionsHandler(HTTPConnection& connection, HTTPRequest& request);
  int traceHandler(HTTPConnection& connection, HTTPRequest& request);

public:
  explicit HTTPSlave(int listener_fd, FileLock& fileLock)
  : epoll_fd(-1), listener_fd(listener_fd), lock(fileLock) {}

  int epollLoop();
};

class HTTPMaster {
private:
  std::vector<int> children;

public:
  explicit HTTPMaster(std::vector<int>& children) : children(children) {}
  HTTPMaster(const HTTPMaster&) = delete;
  HTTPMaster(HTTPMaster&&) = delete;
  HTTPMaster &operator=(const HTTPMaster&) = delete;

  int doJob();

  static int getNumOfCPUCore();
};

#endif //HTTP_SERVER_HTTP_SERVICE_H
