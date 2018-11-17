/*
 * Copyright [2018] <Copyright u35s>
 */

#include <execinfo.h>
#include <cxxabi.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <stack>
#include <memory>
#include <cstdlib>

#include "xlib/trace.h"
#include "xlib/log.h"

namespace xlib {

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

void TraceStack() {
    std::string stack;
    GetStackTrace(&stack);
    TRACE("%s", stack.c_str());
}

void PrintStack() {
    std::string stack;
    GetStackTrace(&stack);
    ERR("%s", stack.c_str());
}

}  // namespace xlib
