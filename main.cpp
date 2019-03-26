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

const int kThreadNum = 50;
const int kTimes = 1;

int counter = kTimes * kThreadNum;

SafeQueue<int> queue;
SafeQueue<int> write_queue;
SafeQueue<int> read_queue;

void reader(int id) {
  while (counter || !queue.empty()) {
    int ret;
    if (queue.pop(ret)) {
      //cout << "Reader " << id << " read: " << ret << endl;
      read_queue.push(ret);
    }
  }
}

void writer(int id) {
  for (int i = 0; i < kTimes; ++i) {
    queue.push(id);
    write_queue.push(id);
    cout << "Writer " << id << " write " << endl;
    __atomic_sub_fetch(&counter, 1, __ATOMIC_RELAXED);
  }
}

int main() {
  std::cout << "Hello, World!" << std::endl;

  ThreadPool readers;
  ThreadPool writers;

  readers.init(kThreadNum);
  writers.init(kThreadNum);

  vector<future<void>> reader_result;
  vector<future<void>> writer_result;

  for (int i = 0; i < kThreadNum; i++) {
    reader_result.push_back(readers.submit(reader, i));
    writer_result.push_back(writers.submit(writer, i));
  }

  for (auto &w: writer_result) {
    w.wait();
  }
  cerr << "all writer finished. writer thread: " << writers.size() << " working: " << writers.workingNum() << endl;
  for (auto &r: reader_result) {
    r.wait();
  }
  cerr << "all reader finished" << endl;

  cout << "result" << endl;

  while (!write_queue.empty()) {
    int ret;
    if (write_queue.pop(ret)) cout << "[" << ret << "]";
  }

  cout << endl;
  vector<int> sort_data;
  while (!read_queue.empty()) {
    int ret;
    if (read_queue.pop(ret)) cout << "[" << ret << "]";
    sort_data.push_back(ret);
  }
  cout << endl;

  sort(sort_data.begin(), sort_data.end());
  for (auto& v : sort_data) {
    cout << "[" << v << "]";
  }
  cout << endl;

  return 0;

  HTTPListener listener;
  listener.listen("0.0.0.0", 8888);

  HTTPConnection connection = listener.accept();

  for (int i = 0; i < 2; i++) {
    HTTPRequest request = connection.recvRequest();
    cout << "Method: " << request.method << endl;
    cout << "Path: " << request.path << endl;
    cout << "HTTP Version: " << request.version_main << "." << request.version_sub << endl;
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

    string payload = "Hello world";
    unordered_map<string, string> headers = {{"Content-Length", to_string(payload.length())}, {"Server", "HL Server"}};
    HTTPResponse response(200, headers, payload.c_str(), payload.length());
    connection.sendResponse(response);
  }

  return 0;
}