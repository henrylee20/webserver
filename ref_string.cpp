//
// Created by henrylee on 19-3-28.
//

#include "ref_string.h"

std::ostream& operator<<(std::ostream& os, const RefString& str) {
  for (char* iter = (char*)str.begin(); iter != str.end(); ++iter) {
    std::cout << (*iter);
  }
  return os;
}

RefString::RefString(const char* begin_p, const char* end_p)
: begin_ptr(begin_p), end_ptr(end_p) {}

RefString::RefString(const char* str)
: begin_ptr(str), end_ptr(str) {
  refTo(str);
}

RefString::RefString(const char* str, size_t len)
: begin_ptr(str), end_ptr(str + len) {}

RefString::RefString(const std::string& str)
: begin_ptr(&(*str.begin())), end_ptr(&(*str.end())){}

void RefString::refTo(const char* begin_p, const char* end_p) {
  begin_ptr = begin_p;
  end_ptr = end_p;
}

void RefString::refTo(const char* str) {
  begin_ptr = str;
  end_ptr = begin_ptr;
  while (*end_ptr) ++end_ptr;
}

void RefString::refTo(const char* str, size_t len) {
  begin_ptr = str;
  end_ptr = str + len;
}

void RefString::refTo(const std::string& str) {
  begin_ptr = &(*str.begin());
  end_ptr = &(*str.end());
}

int RefString::compare (const RefString& str) const {
    const char* p1 = begin_ptr;
    const char* p2 = str.begin();

    for (; p1 != end() && p2 != str.end(); p1++, p2++) {
      if (*p1 < *p2) return -1;
      else if (*p1 > *p2) return 1;
    }

    if (p1 == end() && p2 == str.end()) return 0;
    else if(p1 == end() && p2 != end()) return 1;
    else return -1;
}

int RefString::compare(const std::string& str) const {
  RefString ref_str(str);
  return compare(ref_str);
}

int RefString::compare(const char* str) const {
  const char* str_end = str;
  while (*str_end) ++str_end;

  RefString ref_str(const_cast<char*>(str), const_cast<char*>(str_end));
  return compare(ref_str);
}
