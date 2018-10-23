#ifndef PX_NET_UTIL_H_
#define PX_NET_UTIL_H_

#include <list>
#include <string>
#include <stdint.h>

namespace xlib {
        
#define NETADDR_IP_PRINT_FMT   "%u.%u.%u.%u:%u"
#define NETADDR_IP_PRINT_CTX(socket_info) \
    (socket_info->_ip & 0xFFU), ((socket_info->_ip >> 8) & 0xFFU), \
    ((socket_info->_ip >> 16) & 0xFFU), ((socket_info->_ip >> 24) & 0xFFU), \
    (((socket_info->_port & 0xFF) << 8) | ((socket_info->_port >> 8) & 0xFF))

typedef uint64_t NetAddr;
static const NetAddr INVAILD_NETADDR = UINT64_MAX;
class NetIO;

static const uint8_t TCP_PROTOCOL = 0x80;
static const uint8_t UDP_PROTOCOL = 0x40;
static const uint8_t IN_BLOCKED = 0x10;  
static const uint8_t ADDR_TYPE = 0x07;   
static const uint8_t CONNECT_ADDR = 0x04;
static const uint8_t LISTEN_ADDR = 0x02; 
static const uint8_t ACCEPT_ADDR = 0x01; 

/////////////////////////////////////////////////////////////////////////////////////////////////////

struct SocketInfo {
    void Reset();

    uint8_t GetProtocol() const { return (_state & 0xC0); }
    uint8_t GetAddrType() const { return (_state & 0x7); }

    int32_t _socket_fd;
    uint32_t _addr_info;
    uint32_t _ip;
    uint16_t _port;
    uint8_t _state;
    uint8_t _uin;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

class Epoll
{
    friend class NetIO;
public:
    Epoll();
    ~Epoll();

    int32_t Init(uint32_t max_event);

    int32_t Wait(int32_t timeout_ms);

    int32_t AddFd(int32_t fd, uint32_t events, uint64_t data);

    int32_t DelFd(int32_t fd);

    int32_t ModFd(int32_t fd, uint32_t events, uint64_t data);

    int32_t GetEvent(uint32_t *events, uint64_t *data);

    const char* GetLastError() const {
        return m_last_error;
    }

private:
    char    m_last_error[256];
    int32_t m_epoll_fd;
    int32_t m_max_event;
    int32_t m_event_num;
    struct epoll_event* m_events;
    NetIO*  m_bind_net_io;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

class NetIO {
    friend Epoll;
public:
    NetIO();
    ~NetIO();

    int32_t Init(Epoll* epoll);

    NetAddr Listen(const std::string& ip, uint16_t port);

    NetAddr Accept(NetAddr listen_addr);

    NetAddr ConnectPeer(const std::string& ip, uint16_t port);

    int32_t Send(NetAddr dst_addr, const char* data, uint32_t data_len);

    int32_t SendV(NetAddr dst_addr, uint32_t data_num,
                  const char* data[], uint32_t data_len[], uint32_t offset = 0);

    int32_t SendTo(NetAddr local_addr, NetAddr remote_addr, const char* data, uint32_t data_len);

    int32_t Recv(NetAddr dst_addr, char* buff, uint32_t buff_len);

    int32_t RecvFrom(NetAddr local_addr, NetAddr* remote_addr, char* buff, uint32_t buff_len);

    int32_t Close(NetAddr dst_addr);

    int32_t Reset(NetAddr dst_addr);

    void CloseAll();

    const SocketInfo* GetSocketInfo(NetAddr dst_addr) const;

    const SocketInfo* GetLocalListenSocketInfo(NetAddr dst_addr) const;

    NetAddr GetLocalListenAddr(NetAddr dst_addr);

    const char* GetLastError() const {
        return m_last_error;
    }

    static bool NON_BLOCK;         
    static bool ADDR_REUSE;        
    static bool KEEP_ALIVE;        
    static bool USE_NAGLE;         
    static bool USE_LINGER;        
    static int32_t LINGER_TIME;    
    static int32_t LISTEN_BACKLOG; 
    static uint32_t MAX_SOCKET_NUM;
    static uint8_t AUTO_RECONNECT;

private:
    NetAddr AllocNetAddr();

    void FreeNetAddr(NetAddr net_addr);

    SocketInfo* RawGetSocketInfo(NetAddr net_addr);

    int32_t InitSocketInfo(const std::string& ip, uint16_t port, SocketInfo* socket_info);

    int32_t OnEvent(NetAddr net_addr, uint32_t events);

    int32_t RawListen(NetAddr net_addr, SocketInfo* socket_info);

    int32_t RawConnect(NetAddr net_addr, SocketInfo* socket_info);

    int32_t RawClose(SocketInfo* socket_info);

    char            m_last_error[256];

    NetAddr         m_used_id;
    Epoll*          m_epoll;
    SocketInfo*      m_sockets;
    std::list<NetAddr>  m_free_sockets;
};

}
#endif // PX_NET_UTIL_H_
