//
// Created by henrylee on 19-4-1.
//

#include <fstream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http_service.h"

using namespace std;

int HTTPSlave::epollLoop() {

  // TODO events number
  epoll_event events[128];
  int ret;
  int fd;
  uint32_t event;

  auto pid = getpid();

  if ((epoll_fd = epoll_create(1)) == -1) {
    return -1;
  }

  if (fcntl(listener_fd, F_SETFL, O_NONBLOCK) == -1) {
    fprintf(stderr, "Proc[%d]: fcntl failed\n", pid);
  }

  addEvent(listener_fd, EPOLLIN | EPOLLET | EPOLLEXCLUSIVE);

  while (true) {
    ret = epoll_wait(epoll_fd, events, 128, -1);

    for (int i = 0; i < ret; i++) {
      fd = events[i].data.fd;
      event = events->events;

      printf("Proc[%d]: epoll_wait finished. fd: %d\n", pid, fd);
      if (event & EPOLLRDHUP) {
        rdhupHandler(fd);
      } else {
        if (event & EPOLLIN) {
          if (fd == listener_fd) {
            acceptHandler();
          } else {
            requestHandler(fd);
          }
        }
      }
    }
  }

  return 0;
}

int HTTPSlave::addEvent(int fd, int state) {
  epoll_event event = {0};
  event.events = state;
  event.data.fd = fd;
  return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

int HTTPSlave::delEvent(int fd, int state) {
  epoll_event event = {0};
  event.events = state;
  event.data.fd = fd;
  return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event);
}


int HTTPSlave::modEvent(int fd, int state) {
  epoll_event event = {0};
  event.events = state;
  event.data.fd = fd;
  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}


int HTTPSlave::acceptHandler() {
  int newsock;
  sockaddr_in new_recv_addr{};
  unsigned int addr_len;

  addr_len = sizeof(new_recv_addr);

  newsock = ::accept(listener_fd, (struct sockaddr *)&new_recv_addr, &addr_len);

  if (newsock == -1) {
    auto pid = getpid();
    fprintf(stderr, "Proc[%d]: Connection accept failed.\n", pid);
    return -1;
  }

  uint16_t port = new_recv_addr.sin_port;
  string ip(inet_ntoa(new_recv_addr.sin_addr));

  auto pid = getpid();
  fprintf(stdout, "Proc[%d]: Connection accepted. from: [%s:%u], fd: %d\n", pid, ip.c_str(), port, newsock);

  return addEvent(newsock, EPOLLRDHUP | EPOLLIN | EPOLLET);
}

int HTTPSlave::rdhupHandler(int fd) {
  sockaddr_in new_recv_addr{};
  unsigned int addr_len;
  getsockname(fd, (sockaddr*)&new_recv_addr, &addr_len);
  HTTPConnection connection(fd, inet_ntoa(new_recv_addr.sin_addr), new_recv_addr.sin_port);

  auto pid = getpid();
  fprintf(stdout, "Proc[%d]: closing socket. from: [%s:%u]\n", pid, connection.ip.c_str(), connection.port);
  connection.close();
  return 0;
}

int HTTPSlave::requestHandler(int fd) {
  sockaddr_in new_recv_addr{};
  unsigned int addr_len;
  getsockname(fd, (sockaddr*)&new_recv_addr, &addr_len);
  HTTPConnection connection(fd, inet_ntoa(new_recv_addr.sin_addr), new_recv_addr.sin_port);

  HTTPRequest request;
  uint16_t err_code = 0;

  auto pid = getpid();
  fprintf(stdout, "Proc[%d]: handling request. from: [%s:%u]\n", pid, connection.ip.c_str(), connection.port);

  // TODO May block here
  if (!connection.recvRequest(request, err_code)) {
    if (err_code) {
      errHandler(connection, request, err_code);
      return 0;
    } else {
      // TODO socket read err;
      return -1;
    }
  }

  // Debug Info
  fprintf(stdout, "Proc[%d]: Method: %s\n", pid, to_string(request.method_s).c_str());
  fprintf(stdout, "Proc[%d]: Path: %s\n", pid, request.getDecodedPath().c_str());
//cout << "HTTP Version: " << request.version << endl;
//for (const auto &iter : request.headers) {
//  cout << iter.first << ": " << iter.second << endl;
//}
//cout << "Payload Len: " << request.payload_len << endl;
//cout << "Payload: " << endl;
//if (request.payload_len) {
//  for (int j = 0; j < request.payload_len; j++) {
//    cout << request.payload[j];
//  }
//  cout << endl;
//}

  switch (request.method) {
    case HTTPRequest::kUnknown:
      errHandler(connection, request, 405);
      break;
    case HTTPRequest::kGet:
      getHandler(connection, request);
      break;
    case HTTPRequest::kHead:
      headHandler(connection, request);
      break;
    case HTTPRequest::kPost:
      postHandler(connection, request);
      break;
    case HTTPRequest::kPut:
      putHandler(connection, request);
      break;
    case HTTPRequest::kDelete:
      delHandler(connection, request);
      break;
    case HTTPRequest::kConnect:
      connectHandler(connection, request);
      break;
    case HTTPRequest::kOptions:
      optionsHandler(connection, request);
      break;
    case HTTPRequest::kTrace:
      traceHandler(connection, request);
      break;
  }

  return 0;
}

string HTTPSlave::getGMTime(){
  tm* time_tm;
  time_t timestamp{};

  time(&timestamp);
  time_tm = gmtime(&timestamp);
  string result(asctime(time_tm));
  result.pop_back();
  return result;
}

int HTTPSlave::errHandler(HTTPConnection& connection, HTTPRequest &request, int err_code) {
  unordered_map<string, string> headers(server_info);

  string err_desc = HTTPStatusDesc::getDesc(err_code);
  size_t len = err_desc.length();
  headers["Content-Length"] = to_string(len);
  headers["Date"] = getGMTime();

  auto pid = getpid();
  fprintf(stdout, "Proc[%d]: handling err request. err_code: %d. from: [%s:%u]\n", pid, err_code, connection.ip.c_str(), connection.port);

  HTTPResponse response(err_code, headers, err_desc.c_str(), len);
  // TODO May block here
  connection.sendResponse(response);

  if (!request.is_keep_alive) {
    connection.close();
  }
  return 0;
}

int HTTPSlave::getHandler(HTTPConnection& connection, HTTPRequest &request) {
string path = request.getDecodedPath();
  struct stat stbuff{};
  int fd = open(path.c_str(), O_RDONLY);

  if (fd == -1) {
    return errHandler(connection, request, 404);
  }
  if (fstat(fd, &stbuff) || !S_ISREG(stbuff.st_mode)) {
    return errHandler(connection, request, 500);
  }

  unordered_map<string, string> headers(server_info);
  headers["Date"] = getGMTime();
  headers["Content-Length"] = to_string(stbuff.st_size);
  if (request.is_keep_alive)
    headers["Connection"] = "keep-alive";
  else
    headers["Connection"] = "closed";

  // TODO content-type
  headers["Content-Type"] = HTTPStatusDesc::getContentType(".txt");

  auto pid = getpid();
  fprintf(stdout, "Proc[%d]: handling get request. from: [%s:%u]\n", pid, connection.ip.c_str(), connection.port);

  HTTPResponse response(200, headers, fd, stbuff.st_size);
  // TODO May block here
  connection.sendResponse(response);

  close(fd);
  if (!request.is_keep_alive) {
    connection.close();
  }

  return 0;
}

int HTTPSlave::headHandler(HTTPConnection& connection, HTTPRequest &request) {
  string path = request.getDecodedPath();
  struct stat stbuff{};
  int fd = open(path.c_str(), O_RDONLY);

  if (fd == -1) {
    return errHandler(connection, request, 404);
  }
  if (fstat(fd, &stbuff) || !S_ISREG(stbuff.st_mode)) {
    return errHandler(connection, request, 500);
  }
  close(fd);

  unordered_map<string, string> headers(server_info);
  headers["Date"] = getGMTime();
  headers["Content-Length"] = to_string(stbuff.st_size);
  if (request.is_keep_alive)
    headers["Connection"] = "keep-alive";
  else
    headers["Connection"] = "closed";

  HTTPResponse response(200, headers, "", 0);
  // TODO May block here
  connection.sendResponse(response);

  if (!request.is_keep_alive) {
    connection.close();
  }

  return 0;
}

int HTTPSlave::postHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPSlave::putHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPSlave::delHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPSlave::connectHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPSlave::optionsHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPSlave::traceHandler(HTTPConnection& connection, HTTPRequest &request) {
  // TODO Not Impl
  return errHandler(connection, request, 501);
}

int HTTPMaster::doJob() {
  while(wait(nullptr) != -1);
  return 0;
}

int HTTPMaster::getNumOfCPUCore() {
  int result = 0;
  ifstream ios("/proc/cpuinfo");

  if (!ios.is_open()) return 0;
  string line;

  while (!ios.eof()) {
    ios >> line;    // Try to read "processor"
    auto pos = line.find("processor");
    if (pos== string::npos) continue;

    ios >> line;    // Skip ':'
    ios >> result;  // Get the Core ID
  }
  // Finish loop to get max ID

  return result;
}
