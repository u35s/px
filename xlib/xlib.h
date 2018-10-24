#ifndef XLIB_XLIB_H_
#define XLIB_XLIB_H_

#include <netinet/in.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace xlib {

void split(const std::string& text, const std::string& sep, std::vector<std::string> strs);

int atoi(char* a);

int connect_to(struct sockaddr_in* addr);

int connect_to(char* host, int port);

int get_ip_by_domain(const char *domain, char *ip);

void print_stack();

class Buffer {
 public:
    Buffer(const char *bytes, int len);
    virtual ~Buffer();
    char* Bytes();
    int Length();
    void Add(int n);
 private:
    char *data_;
    int len_;
    int pos_;
};

}  // namespace xlib

#endif  // XLIB_XLIB_H_
