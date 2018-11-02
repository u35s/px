/*
 * Copyright [2018] <Copyright u35s>
 */

#include <sys/ioctl.h>
#include <netdb.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <stack>
#include <memory>
#include <cstdlib>

#include "xlib/xlib.h"
#include "xlib/log.h"

namespace xlib {

void Split(const std::string& str,
    const std::string& delim,
    std::vector<std::string>* result) {
    if (str.empty()) {
        return;
    }
    if (delim[0] == '\0') {
        result->push_back(str);
        return;
    }

    size_t delim_length = delim.length();

    for (std::string::size_type begin_index = 0; begin_index < str.size();) {
        std::string::size_type end_index = str.find(delim, begin_index);
        if (end_index == std::string::npos) {
            result->push_back(str.substr(begin_index));
            return;
        }
        if (end_index > begin_index) {
            result->push_back(str.substr(begin_index, (end_index - begin_index)));
        }

        begin_index = end_index + delim_length;
    }
}

int Atoi(char* a) { return std::atoi(a); }

int GetIpByDomain(const char *domain, char *ip) {
    char **pptr;
    struct hostent *hptr;

    hptr = gethostbyname(domain);
    if (NULL == hptr) {
        hptr = gethostbyname(domain);
        if (NULL == hptr) {
            return -1;
        }
    }
    for (pptr = hptr->h_addr_list ; *pptr != NULL; pptr++) {
        if (NULL != inet_ntop(hptr->h_addrtype, *pptr, ip, 16)) {
            return 0;  // 只获取第一个 ip
        }
    }
    return -1;
}

void DemangleSymbol(std::string* symbol) {
    size_t size = 0;
    int status = -4;
    char temp[256] = {'\0'};
    if (sscanf(symbol->c_str(), "%*[^(_]%[^ )+]", temp) == 1) {
        std::unique_ptr<char, void(*)(void*)> demangled {
            abi::__cxa_demangle(temp, NULL, &size, &status),
            std::free
        };
        if (demangled.get()) {
            auto start = symbol->find(temp);
            symbol->replace(start, strlen(temp), "");
            symbol->insert(start, demangled.get());
            return;
        }
    }
}

void GetStackTrace(std::string* stack) {
    void* addresses[1024];
    int size = backtrace(addresses, 1024);
    std::unique_ptr<char*, void(*)(void*)> symbols {
        backtrace_symbols(addresses, size),
        std::free
    };
    for (int i = 0; i < size; ++i) {
        std::string demangled(symbols.get()[i]);
        DemangleSymbol(&demangled);
        stack->append(demangled);
        stack->append("\n");
    }
}

void PrintStack() {
    std::string stack;
    GetStackTrace(&stack);
    ERR("%s", stack.c_str());
}

Buffer::Buffer(const char *bytes, int len):
    len_(len), pos_(0) {
    data_ = new char[len_+1];
    memcpy(data_, bytes, len_);
    data_[len_] = '\0';
}

Buffer::~Buffer()        { delete [] data_; }

char* Buffer::Bytes()    { return data_+pos_; }
int   Buffer::Length()   { return len_ - pos_; }
void  Buffer::Add(int n) { pos_ += n; }

}  // namespace xlib
