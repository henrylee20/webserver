//
// Created by henrylee on 19-1-4.
//

#ifndef HTTP_SERVER_HTTP_H
#define HTTP_SERVER_HTTP_H

#include <string>
#include <unordered_map>
#include <memory>

#include "ref_string.h"

static constexpr char kHTTPVersion[] = "HTTP/1.1";

class HTTPStatusDesc {
private:
  std::unordered_map<uint16_t , std::string> code_desc = {
      {100, "Continue"},
      {101, "Switching Protocols"},

      {200, "OK"},
      {201, "Created"},
      {202, "Accepted"},
      {203, "Non-Authoritative Information"},
      {204, "No Content"},
      {205, "Reset Content"},
      {206, "Partial Content"},

      {300, "Multiple Choices"},
      {301, "Moved Permanently"},
      {302, "Found"},
      {303, "See Other"},
      {304, "Not Modified"},
      {305, "Use Proxy"},
      {306, "Unused"},
      {307, "Temporary Redirect"},

      {400, "Bad Request"},
      {401, "Unauthorized"},
      {402, "Payment Required"},
      {403, "Forbidden"},
      {404, "Not Found"},
      {405, "Method Not Allowed"},
      {406, "Not Acceptable"},
      {407, "Proxy Authentication Required"},
      {408, "Request Time-out"},
      {409, "Conflict"},
      {410, "Gone"},
      {411, "Length Required"},
      {412, "Precondition Failed"},
      {413, "Request Entity Too Large"},
      {414, "Request-URI Too Large"},
      {415, "Unsupported Media Type"},
      {416, "Requested range not satisfiable"},
      {417, "Expectation Failed"},

      {500, "Internal Server Error"},
      {501, "Not Implemented"},
      {502, "Bad Gateway"},
      {503, "Service Unavailable"},
      {504, "Gateway Time-out"},
      {505, "HTTP Version not supported"},
  };

  static HTTPStatusDesc instance;

private:
  HTTPStatusDesc() = default;
  HTTPStatusDesc(const HTTPStatusDesc&) = default;
  HTTPStatusDesc& operator=(const HTTPStatusDesc&) = default;

public:
  static std::string getDesc(uint16_t code);
};

class HTTPRequest {
public:
  RefString method;
  RefString path;
  RefString version;
  std::unordered_map<RefString, RefString> headers;
  size_t http_header_len;
  size_t payload_len;
  char* payload;

public:
  HTTPRequest();
  HTTPRequest(const HTTPRequest& request);
  HTTPRequest(HTTPRequest&& request) noexcept;

  HTTPRequest& operator=(const HTTPRequest &request) = delete;

  ~HTTPRequest();
  bool parseHeader(char* header_data);
  bool setPayload(char* payload_ptr);
  std::string getDecodedPath();

private:
  char* header_buf;
  char* payload_buf;

private:
  char* parseRequestMethod(char* p);
  char* parseRequestPath(char* p);
  char* parseRequestVersion(char* p);
  char* parseRequestLine(char* p);
  char* parseHeaderLine(char* p);
  char* parseHeaders(char* p);
};

class HTTPResponse {
private:
  uint16_t code;
  std::string self_desc;
  std::unordered_map<std::string, std::string> headers;
  size_t payload_len;
  size_t http_header_len;

  const char* payload;

public:
  HTTPResponse(uint16_t code, std::unordered_map<std::string, std::string> headers, const char* payload, size_t payload_len);
  HTTPResponse(uint16_t code, std::string desc, std::unordered_map<std::string, std::string> headers, const char* payload, size_t payload_len);

  char* getRawData();
  size_t getRawDataLen();
};

#endif //HTTP_SERVER_HTTP_H
