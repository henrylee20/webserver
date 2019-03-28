#include <utility>

#include <utility>

#include <utility>

//
// Created by henrylee on 19-1-4.
//

#include <string>
#include <unordered_map>
#include <memory>
#include <string.h>
#include <sstream>
#include <cassert>
#include "http.h"

using namespace std;

HTTPStatusDesc HTTPStatusDesc::instance;
std::string HTTPStatusDesc::getDesc(uint16_t code) {
  auto iter = instance.code_desc.find(code);
  if (iter != instance.code_desc.end()) {
    return iter->second;
  } else {
    return "";
  }
}

HTTPResponse::HTTPResponse(uint16_t code, unordered_map<string, string> headers, const char *payload, size_t payload_len)
                           : code(code), headers(std::move(headers)), payload(payload), payload_len(payload_len) {
}

HTTPResponse::HTTPResponse(uint16_t code, string desc, unordered_map<string, string> headers, const char *payload, size_t payload_len)
                           : code(code), self_desc(std::move(desc)), headers(std::move(headers)), payload(payload), payload_len(payload_len) {
}

char* HTTPResponse::getRawData() {
  stringstream response_info;
  string code_desc = HTTPStatusDesc::getDesc(code);

  if (code_desc.empty()) {
    code_desc = self_desc;
  }

  response_info << kHTTPVersion << " " << code << " " << code_desc << "\r\n";
  for (const auto &iter : headers) {
    response_info << iter.first << ": " << iter.second << "\r\n";
  }
  response_info << "\r\n";

  string response = response_info.str();
  http_header_len = response.length();

  char* result = new char[http_header_len + payload_len];
  strcpy(result, response.c_str());
  if (payload_len) {
    memcpy(result + response.length(), payload, payload_len);
  }

  return result;
}

size_t HTTPResponse::getRawDataLen() {
  return http_header_len + payload_len;
}

HTTPRequest::HTTPRequest()
: http_header_len(0), payload_len(0), payload(nullptr), header_buf(nullptr), payload_buf(nullptr) {
}

HTTPRequest::HTTPRequest(const HTTPRequest& request)
: method(request.method), path(request.path), version(request.version), headers(request.headers)
, http_header_len(request.http_header_len), payload_len(request.payload_len)
, payload(nullptr), header_buf(nullptr), payload_buf(nullptr) {
  header_buf = new char[request.http_header_len];
  memcpy(header_buf, request.header_buf, request.http_header_len);
  if (request.payload_len) {
    payload_buf = new char[request.payload_len];
    memcpy(payload_buf, request.payload, request.payload_len);
    payload = payload_buf;
  }
}

HTTPRequest::HTTPRequest(HTTPRequest&& request) noexcept
: method(request.method), path(request.path), version(request.version), headers(request.headers)
, http_header_len(request.http_header_len), payload_len(request.payload_len)
, payload(request.payload), header_buf(request.header_buf), payload_buf(request.payload_buf) {
  request.payload = nullptr;
  request.header_buf = nullptr;
  request.payload_buf = nullptr;
}

HTTPRequest::~HTTPRequest() {
  delete[] header_buf;
  delete[] payload_buf;
}

bool HTTPRequest::parseHeader(char* header_data) {
  header_buf = header_data;
  char* p = header_buf;

  p = parseRequestLine(p);
  if (p == nullptr) {
    return false;
  }
  p = parseHeaders(p);
  if (p == nullptr) {
    return false;
  }

  http_header_len = p - header_data;

  auto len_str = headers.find("Content-Length");
  if (len_str != headers.end()) {
    this->payload_len = stoul(string(len_str->second.begin(), len_str->second.length()));
    this->payload = p;
  } else {
    this->payload_len = 0;
    this->payload = nullptr;
  }

  return true;
}

bool HTTPRequest::setPayload(char* payload_ptr) {
  payload_buf = payload_ptr;
  payload = payload_buf;
  return true;
}


std::string HTTPRequest::getDecodedPath() {
  stringstream ss;
  char hex_trans_buf[3] = {0};
  for (auto i = path.begin(); i != path.end(); i++) {
    if (*i == '%' &&
    ((*(i + 1) >= '0' && *(i + 1) <= '9') || (*(i + 1) >= 'a' && *(i + 1) <= 'f') || (*(i + 1) >= 'A' && *(i + 1) <= 'F')) &&
    ((*(i + 2) >= '0' && *(i + 2) <= '9') || (*(i + 2) >= 'a' && *(i + 2) <= 'f') || (*(i + 2) >= 'A' && *(i + 2) <= 'F'))) {
      hex_trans_buf[0] = *(i + 1);
      hex_trans_buf[1] = *(i + 2);
      hex_trans_buf[2] = '\0';
      ss << (char)strtol(hex_trans_buf, nullptr, 16);
      i += 2;
    } else {
      ss << *i;
    }
  }

  return ss.str();
}

char *HTTPRequest::parseRequestMethod(char *p) {
  size_t i = 0;
  while (p[i] != ' ') {
    if (!p[i]) return nullptr;
    i++;
  }
  method = RefString(p, i);
  p += i;

  while (*p == ' ') {
    if (!*p) return nullptr;
    p++;
  }

  return p;
}

char *HTTPRequest::parseRequestPath(char *p) {
  size_t i = 0;
  for (i = 0; p[i] != ' '; i++) {
    if (!p[i]) return nullptr;
  }

  path = RefString(p, i);

  p += i;
  while (*p == ' ') {
    if (!*p) return nullptr;
    p++;
  }

  return p;
}

char *HTTPRequest::parseRequestVersion(char *p) {
  if (!(*p == 'H' && *(p + 1) == 'T' && *(p + 2) == 'T' && *(p + 3) == 'P' && *(p + 4) == '/')) {
    return nullptr;
  }
  p += 5;
  version = RefString(p, 3);
  p += 3;
  return p;
}

char *HTTPRequest::parseRequestLine(char *p) {
  if (!p || !*p) {
    return nullptr;
  }

  p = parseRequestMethod(p);
  if (p == nullptr) {
    return nullptr;
  }

  p = parseRequestPath(p);
  if (p == nullptr) {
    return nullptr;
  }

  p = parseRequestVersion(p);
  if (p == nullptr) {
    return nullptr;
  }

  while (*p == '\r' || *p == '\n' || *p == ' ') {
    if (!*p) return nullptr;
    p++;
  }

  return p;
}

char *HTTPRequest::parseHeaderLine(char *p) {
  size_t i = 0;
  char* start_pos = nullptr;

  //cerr<< "header key: [" << p << "]" <<endl;

  start_pos = p;
  for (i = 0; *p != ':'; i++, p++) {
    assert(*p);
    if (!*p) return nullptr;
  }
  i--;
  p++;

  while (start_pos[i] == ' ') i--;
  while (*p == ' ') {
    assert(*p);
    if (!*p) return nullptr;
    p++;
  }
  RefString key(start_pos, i + 1);

  //cerr<< "header val: [" << p << "]" <<endl;
  start_pos = p;
  for (i = 0; !(*p == '\r' && *(p + 1) == '\n'); i++, p++) {
    assert(*p);
    if (!*p) return nullptr;
  }
  i--;
  p += 2;

  while (start_pos[i] == ' ') i--;
  RefString val(start_pos, i + 1);

  headers.insert(make_pair(key, val));
  return p;
}

char *HTTPRequest::parseHeaders(char *p) {
  while (!(*p == '\r' && *(p + 1) == '\n')) {
    p = parseHeaderLine(p);
    if (!*p) return nullptr;
  }
  p += 2;
  return p;
}
