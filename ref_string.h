//
// Created by henrylee on 19-3-27.
//

#ifndef HTTP_SERVER_REF_STRING_H
#define HTTP_SERVER_REF_STRING_H

#include <cstddef>
#include <iostream>

class RefString {
private:
  const char* begin_ptr;
  const char* end_ptr;

public:
  RefString()
  : begin_ptr(nullptr), end_ptr(nullptr) {}

  RefString(const RefString& str) = default;
  RefString(RefString&& str) = default;
  RefString& operator=(const RefString& str) = default;

  RefString(const char* begin_p, const char* end_p);
  explicit RefString(const std::string& str);
  RefString(const char* str);
  RefString(const char* str, size_t len);

  void refTo(const char* begin_p, const char* end_p);
  void refTo(const std::string& str);
  void refTo(const char* str);
  void refTo(const char* str, size_t len);

  const char* begin() { return begin_ptr;}
  const char* begin() const { return begin_ptr;}
  const char* end() { return end_ptr;}
  const char* end() const { return end_ptr;}

  size_t length() { return end_ptr - begin_ptr;}
  const size_t length() const { return end_ptr - begin_ptr;}

  const char& at(size_t pos) { return begin_ptr[pos];}
  const char& at(size_t pos) const { return begin_ptr[pos];}

  const char& operator[](size_t pos) { return at(pos);}
  const char& operator[](size_t pos) const { return at(pos);}

  int compare (const RefString& str) const;
  int compare(const std::string& str) const;
  int compare(const char* str) const;

  bool operator==(const RefString& str) const { return !compare(str);}
  bool operator==(const std::string& str) const { return !compare(str);}
  bool operator==(const char* str) const { return !compare(str);}

  std::string toString() { return std::string(begin_ptr, end_ptr - begin_ptr);}

  friend std::ostream& operator<<(std::ostream& os, const RefString& str);
};

template <>
struct std::hash<RefString>
    : public __hash_base<size_t, RefString>{
  size_t operator()(const RefString& str) const {
    return std::_Hash_impl::hash(str.begin(), str.length());
  }
};

#endif //HTTP_SERVER_REF_STRING_H
