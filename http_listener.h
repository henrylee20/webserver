//
// Created by henrylee on 19-1-8.
//

#ifndef HTTP_SERVER_TCPLISTENNER_H
#define HTTP_SERVER_TCPLISTENNER_H

#include <cstdint>
#include <string>
#include <memory>
#include <pthread.h>
#include "http.h"

class HTTPConnection {
private:
  int fd;

public:
  std::string ip;
  uint16_t port;

private:
  ssize_t read(char* buf, size_t max_len);
  ssize_t write(const char* buf, size_t len);

public:
  HTTPConnection();
  HTTPConnection(int fd, std::string ip, uint16_t port);
  bool recvRequest(HTTPRequest& request, uint16_t& err_code);
  bool sendResponse(HTTPResponse& response);
  void close();
};

class HTTPListener {
private:
  int master_fd;
  std::string ip;
  uint16_t port;

public:
  HTTPListener () = default;
  explicit HTTPListener(int fd) : master_fd(fd), port(0) {}
  bool listen(std::string ip, uint16_t port);
  HTTPConnection accept();
  int getFD() { return master_fd;}
};


#endif //HTTP_SERVER_TCPLISTENNER_H
