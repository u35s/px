/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_XLIB_H_
#define XLIB_XLIB_H_

#include <netinet/in.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace xlib {

void Split(const std::string& text, const std::string& sep, std::vector<std::string>* strs);

int Atoi(char* a);

int GetIpByDomain(const char *domain, char *ip);

void PrintStack();

void TraceStack();

class Buffer {
 public:
    Buffer(const char *bytes, int len);
    ~Buffer();
    char* Bytes();
    int   Length();
    void  Add(int n);
 private:
    char *data_;
    int  len_;
    int  pos_;
};

}  // namespace xlib

#endif  // XLIB_XLIB_H_
