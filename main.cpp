#include <iostream>
#include <typeinfo>
#include <thread>
#include <vector>
#include <list>
#include <mutex>
#include <algorithm>

#include <pthread.h>

#include "http.h"
#include "http_listener.h"
#include "queue.h"
#include "thread_pool.h"

using namespace std;

void process(HTTPConnection& connection) {
  HTTPRequest request;
  uint16_t err_code;
  if (!connection.recvRequest(request, err_code)) {
    if (err_code) {
      // TODO send err response;
    } else {
      // TODO socket read err;
      connection.close();
      return;
    }
  }

  cout << "Method: " << request.method << endl;
  cout << "Path: " << request.getDecodedPath() << endl;
  cout << "HTTP Version: " << request.version << endl;
  for (const auto &iter : request.headers) {
    cout << iter.first << ": " << iter.second << endl;
  }
  cout << "Payload Len: " << request.payload_len << endl;
  cout << "Payload: " << endl;
  if (request.payload_len) {
    for (int j = 0; j < request.payload_len; j++) {
      cout << request.payload[j];
    }
    cout << endl;
  }

  // TODO handle Keep-Alive

  // TODO construct a response
  string payload = "Hello world";
  unordered_map<string, string> headers = {{"Content-Length", to_string(payload.length())}, {"Server", "HL Server"}};
  HTTPResponse response(200, headers, payload.c_str(), payload.length());
  connection.sendResponse(response);
  connection.close();
}


int main() {
  std::cout << "Hello, World!" << std::endl;
  HTTPListener listener;
  listener.listen("0.0.0.0", 8888);

  ThreadPool threads;
  threads.init(1);

  for (int i = 0; i < 3; i++) {
    threads.submit(process, listener.accept());
  }

  return 0;
}