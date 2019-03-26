//
// Created by henrylee on 19-1-8.
//

#include <utility>
#include <string>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "http_listener.h"

using namespace std;

constexpr size_t kRecvBufSize = 4096;

bool HTTPListener::listen(std::string ip, uint16_t port) {
  this->port = port;
  this->ip = std::move(ip);

  int sd;
  sockaddr_in addr{};
  int ret_val;
  int flag;

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd < 0) {
    perror("init(): socket() error!");
    return false;
  }
  flag = true;
  ret_val = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*) &flag, sizeof(flag));

  if (ret_val == -1) {
    perror("init(): setsockopt(SO_REUSEADDR) error!");
    return false;
  }

  addr.sin_family = AF_INET;
  //addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_addr.s_addr = inet_addr(this->ip.c_str());
  addr.sin_port = htons(port);

  if((bind(sd, (struct sockaddr*) &addr, sizeof(addr))) == -1) {
    perror("init(): bind() error!");
    return false;
  }

  if ((::listen(sd, 20)) == -1) {
    perror("init(): listen() error!");
    return false;
  }

  if (pthread_mutex_init(&mutex_accept, nullptr) < 0) {
    printf("Failed to init connection mutex");
  }

  master_fd = sd;

  return true;
}

HTTPConnection HTTPListener::accept() {
  if (pthread_mutex_lock(&mutex_accept) < 0) {
  		printf("Failed to lock connection mutex.");
  }

  int newsock;
  sockaddr_in new_recv_addr{};
  unsigned int addr_len;

  addr_len = sizeof(new_recv_addr);

  newsock = ::accept(master_fd, (struct sockaddr *)&new_recv_addr, &addr_len);

  if (pthread_mutex_unlock(&mutex_accept) < 0) {
  		printf("Failed to unlock connection mutex.");
  }

  uint16_t port = new_recv_addr.sin_port;
  string ip(inet_ntoa(new_recv_addr.sin_addr));

  return std::move(HTTPConnection(newsock, ip, port));
}

HTTPConnection::HTTPConnection(int fd, string ip, uint16_t port): fd(fd), ip(std::move(ip)), port(port) {
}

ssize_t HTTPConnection::read(char *buf, size_t max_len) {
  return ::read(fd, buf, max_len);
}

ssize_t HTTPConnection::write(char *buf, size_t len) {
  ssize_t sent_len = 0;
  ssize_t n;
  while (sent_len != len) {
    n = ::write(fd, buf + sent_len, len - sent_len);
    if (n < 0) {
      return n;
    }
    sent_len += n;
  }
  return sent_len;
}

HTTPRequest HTTPConnection::recvRequest() {
  shared_ptr<char> buf(new char[kRecvBufSize]);

  ssize_t read_len = read(buf.get(), kRecvBufSize);
  HTTPRequest request;
  request.parseData(buf);

  if (read_len == kRecvBufSize && request.payload_len == 0) {
    perror("Headers is too large.\n");
  }

  if (request.payload_len && read_len == kRecvBufSize) {
    shared_ptr<char> new_buf(new char[kRecvBufSize + request.payload_len]);
    memcpy(new_buf.get(), buf.get(), kRecvBufSize);
    request.changeBuf(new_buf);

    size_t left_size = request.payload_len + request.http_header_len - kRecvBufSize;
    read(new_buf.get() + kRecvBufSize, left_size);
  }

  return move(request);
}

bool HTTPConnection::sendResponse(HTTPResponse &response) {
  auto ptr = response.getRawData();
  size_t len = response.getRawDataLen();
  if (write(ptr.get(), len) < 0) {
    perror("Send response error\n");
    return false;
  }
  return true;
}

void HTTPConnection::close() {
  ::close(fd);
}
