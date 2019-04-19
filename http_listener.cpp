//
// Created by henrylee on 19-1-8.
//

#include <utility>
#include <string>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
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

  master_fd = sd;

  return true;
}

HTTPConnection HTTPListener::accept() {
  int newsock;
  sockaddr_in new_recv_addr{};
  unsigned int addr_len;

  addr_len = sizeof(new_recv_addr);

  newsock = ::accept(master_fd, (struct sockaddr *)&new_recv_addr, &addr_len);

  uint16_t port = new_recv_addr.sin_port;
  string ip(inet_ntoa(new_recv_addr.sin_addr));

  return std::move(HTTPConnection(newsock, ip, port));
}

HTTPConnection::HTTPConnection() : fd(-1), ip("Not Connected"), port(0) {}
HTTPConnection::HTTPConnection(int fd, string ip, uint16_t port): fd(fd), ip(std::move(ip)), port(port) {}

ssize_t HTTPConnection::read(char *buf, size_t max_len) {
  return ::read(fd, buf, max_len);
}

ssize_t HTTPConnection::write(const char *buf, size_t len) {
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

bool HTTPConnection::recvRequest(HTTPRequest& request, uint16_t& err_code) {
  char* header_buf = new char[kRecvBufSize];

  err_code = 0;
  ssize_t read_len = read(header_buf, kRecvBufSize);
  auto pid = getpid();

  if (read_len == 0) {
    fprintf(stdout, "Proc[%d]: socket read finished.\n", pid);
    return false;
  }

  if (read_len < 0) {
    fprintf(stderr, "Proc[%d]: socket invalid.\n", pid);
    return false;
  }

  fprintf(stdout, "Proc[%d]: recv len: %ld\n", pid, read_len);

  if (!request.parseHeader(header_buf)) {
    fprintf(stderr, "Proc[%d]: Parse Header err.\n", pid);
    err_code = 400;
    return false;
  }

  if (read_len == kRecvBufSize && request.payload_len == 0) {
    fprintf(stderr, "Proc[%d]: Headers is too large.\n", pid);
    err_code = 413;
    return false;
  }

  if (request.payload_len && read_len == kRecvBufSize) {
    auto payload_buf = new char[request.payload_len];

    size_t offset = kRecvBufSize - request.http_header_len;
    memcpy(payload_buf, request.payload, offset);

    size_t left_size = request.payload_len - offset;
    read(payload_buf + offset, left_size);

    request.setPayload(payload_buf);
  }

  return true;
}

bool HTTPConnection::sendResponse(HTTPResponse &response) {
  if (response.getPayloadFD() < 0) {

    auto ptr = response.getRawData();
    size_t len = response.getRawDataLen();
    if (write(ptr, len) < 0) {
      perror("Send response error\n");
      delete[] ptr;
      return false;
    }
    delete[] ptr;
    return true;

  } else {

    auto header_data = response.getHeaderRawDate();
    if (write(header_data.c_str(), header_data.length()) < 0) {
      perror("Send response error\n");
      return false;
    }
    size_t len = response.getPayloadLen();
    size_t sent_len = 0;
    int payload_fd = response.getPayloadFD();
    while (sent_len != len) {
      ssize_t n = sendfile(fd, payload_fd, nullptr, len - sent_len);
      if (n == -1) {
        perror("sendfile failed.\n");
        return false;
      }
      sent_len += n;
    }
    return true;
  }
}

void HTTPConnection::close() {
  ::close(fd);
}
