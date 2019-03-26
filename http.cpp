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

HTTPRequest::HTTPRequest() :
version_main(0), version_sub(0), payload(nullptr), payload_len(0), http_header_len(0) {
}

char *HTTPRequest::parseRequestMethod(char *p) {
  size_t i = 0;
  while (p[i] != ' ') {
    if (!p[i]) return nullptr;
    i++;
  }
  method = string(p, i);
  p += i;

  while (*p == ' ') {
    if (!*p) return nullptr;
    p++;
  }

  return p;
}

char *HTTPRequest::parseRequestPath(char *p) {
  int i = 0;
  stringstream ss;
  char hex_trans_buf[3] = {0};
  for (i = 0; p[i] != ' '; i++) {
    if (!p[i]) return nullptr;

    if (p[i] == '%' &&
    ((p[i + 1] >= '0' && p[i + 1] <= '9') || (p[i + 1] >= 'a' && p[i + 1] <= 'f') || (p[i + 1] >= 'A' && p[i + 1] <= 'F')) &&
    ((p[i + 2] >= '0' && p[i + 2] <= '9') || (p[i + 2] >= 'a' && p[i + 2] <= 'f') || (p[i + 2] >= 'A' && p[i + 2] <= 'F'))) {
      hex_trans_buf[0] = p[i + 1];
      hex_trans_buf[1] = p[i + 2];
      hex_trans_buf[2] = '\0';
      ss << (char)strtol(hex_trans_buf, nullptr, 16);
      i += 2;
    } else {
      ss << p[i];
    }
  }

  path = ss.str();

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

  version_main = *p - '0';
  p += 2;

  version_sub = *p - '0';
  p++;

  return p;
}

char* HTTPRequest::parseRequestLine(char *p){
  if (!*p) {
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

char* HTTPRequest::parseHeaderLine(char *p) {
  size_t i = 0;
  char* start_pos = nullptr;

  start_pos = p;
  for (i = 0; *p != ':'; i++, p++) {
    if (!*p) return nullptr;
  }
  i--;
  p++;

  while (start_pos[i] == ' ') i--;
  while (*p == ' ') {
    if (!*p) return nullptr;
    p++;
  }
  string key(start_pos, i + 1);

  start_pos = p;
  for (i = 0; !(*p == '\r' && *(p + 1) == '\n'); i++, p++) {
    if (!*p) return nullptr;
  }
  i--;
  p += 2;

  while (start_pos[i] == ' ') i--;
  string val(start_pos, i + 1);

  this->headers[key] = val;
  return p;
}

char* HTTPRequest::parseHeaders(char *p) {
  while (!(*p == '\r' && *(p + 1) == '\n')) {
    p = parseHeaderLine(p);
    if (!*p) return nullptr;
  }
  p += 2;
  return p;
}

bool HTTPRequest::parseData(shared_ptr<char> raw_data) {
  this->raw_ptr = std::move(raw_data);
  char* p = raw_ptr.get();

  p = parseRequestLine(p);
  if (p == nullptr) {
    return false;
  }
  p = parseHeaders(p);
  if (p == nullptr) {
    return false;
  }

  auto len_str = headers.find("Content-Length");
  if (len_str != headers.end()) {
    this->payload_len = stoul(len_str->second);
    this->payload = p;
  } else {
    this->payload_len = 0;
    this->payload = nullptr;
  }

  this->http_header_len = this->payload - raw_data.get();

  return true;
}

void HTTPRequest::changeBuf(std::shared_ptr<char> new_buf) {
  this->raw_ptr = std::move(new_buf);
  this->payload = this->raw_ptr.get() + http_header_len;
}


HTTPResponse::HTTPResponse(uint16_t code, unordered_map<string, string> headers, const char *payload, size_t payload_len)
                           : code(code), headers(std::move(headers)), payload(payload), payload_len(payload_len) {
}

HTTPResponse::HTTPResponse(uint16_t code, string desc, unordered_map<string, string> headers, const char *payload, size_t payload_len)
                           : code(code), self_desc(std::move(desc)), headers(std::move(headers)), payload(payload), payload_len(payload_len) {
}

std::shared_ptr<char> HTTPResponse::getRawData() {
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

  shared_ptr<char> result(new char[http_header_len + payload_len]);
  strcpy(result.get(), response.c_str());
  if (payload_len) {
    memcpy(result.get() + response.length(), payload, payload_len);
  }

  return std::move(result);
}

size_t HTTPResponse::getRawDataLen() {
  return http_header_len + payload_len;
}



