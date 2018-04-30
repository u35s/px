#include <sys/ioctl.h> //FIONBIO
#include <cstdlib> //std::atoi  std::perror
#include <netdb.h>  //gethostname
#include <execinfo.h> // backtrace
#include <cxxabi.h>
#include <arpa/inet.h> //inet_addr inet_ntop
#include <iostream>
#include <boost/algorithm/string.hpp> //boost::split
#include "xlib.h"

namespace xlib {

void split(const std::string& text,
	       const std::string& sep,
		   std::vector<std::string>& strs){
    boost::split(strs,text,boost::is_any_of(sep),boost::token_compress_on);
}

int atoi(char* a) { return std::atoi(a); }

int connect_to(struct sockaddr_in* addr) {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    unsigned long ul = 3;
    ioctl(fd, FIOASYNC, &ul);
	if (connect(fd, (struct sockaddr *)addr, sizeof(*addr)) != 0) {
        std::perror("connect");
	}
	return fd;
}

int connect_to(char* host,int port) {
    struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host);
    return connect_to(&addr);
}


int get_ip_by_domain(const char *domain, char *ip)  
{  
    char **pptr;  
    struct hostent *hptr;  
  
    hptr = gethostbyname(domain);  
    if(NULL == hptr){  
        return -1;  
    }  
    for(pptr = hptr->h_addr_list ; *pptr != NULL; pptr++) {  
        if (NULL != inet_ntop(hptr->h_addrtype, *pptr, ip, 16) ) {  
            return 0; // 只获取第一个 ip  
        }  
    }  
    return -1;  
}  

void demangle_symbol(std::string* symbol){
    size_t size = 0;  
    int status = -4;
    char temp[256] = {'\0'};
    //first, try to demangle a c++ name
    if (int sz = sscanf(symbol->c_str(), "%*[^(_]%[^ )+]", temp) == 1) {
        std::unique_ptr<char, void(*)(void*)> demangled {
            abi::__cxa_demangle(temp, NULL, &size, &status),
            std::free
        };
        if (demangled.get()) {
			auto start = symbol->find(temp);
			symbol->replace(start,strlen(temp),"");
            symbol->insert(start,demangled.get());
            return;
        }
	}
    //if that didn't work, try to get a regular c symbol
    //if (1 == sscanf(symbol->c_str(), "%255s", temp)) {
	//	symbol->clear();
    //    symbol->append(temp);
    //}
}

void get_stack_trace(std::string* stack)
{
    void* addresses[1024];
    int size = backtrace(addresses, 1024);
    std::unique_ptr<char*, void(*)(void*)> symbols {
        backtrace_symbols(addresses, size),
        std::free
    };
    for (int i = 0; i < size; ++i) {
        std::string demangled(symbols.get()[i]);
        demangle_symbol(&demangled);
        stack->append(demangled);
        stack->append("\n");
    }
}

void print_stack() { 
    std::string stack; 
	get_stack_trace(&stack);
	std::cout << stack << std::endl; 
}

Buffer::Buffer(const char *bytes, int len):
    len_(len),pos_(0){
    data_ = new char[len_+1];
    memcpy(data_, bytes, len_);
    data_[len_] = '\0';
}

Buffer::~Buffer() { delete [] data_; }
char* Buffer::Bytes() { return data_+pos_; }
int Buffer::Length() { return len_ - pos_; }
void Buffer::Add(int n) { pos_ += n; }

} //end namespace xlib