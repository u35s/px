/*
 * Copyright [2018] <Copyright u35s>
 */

#include <string.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include "xlib/xlib.h"
#include "xlib/log.h"
#include "server/ps.h"
#include "server/pc.h"

ProxyClient::ProxyClient(const uint64_t handle, xlib::NetIO* netio)
    : m_reading_wait(false), m_peer_reading_wait(false),
      m_handle(handle), m_peer_handle(0),
      m_recv_data_len(0), m_send_data_len(0), first_line_read_(false), parsed_(false),
      m_netio(netio) {
}

ProxyClient::~ProxyClient() {
    auto socket_info = m_netio->GetSocketInfo(m_handle);

    INF("client ip " NETADDR_IP_PRINT_FMT ", remote host %s, recv %d, send %d,",
        NETADDR_IP_PRINT_CTX(socket_info), m_remote_host.c_str(),
        m_recv_data_len, m_send_data_len);
    m_netio->Close(m_handle);
    m_netio->Close(m_peer_handle);

    while (!remote_write_queue_.empty()) {
        xlib::Buffer* buffer = remote_write_queue_.front();
        remote_write_queue_.pop_front();
        delete buffer;
    }
    while (!client_write_queue_.empty()) {
        xlib::Buffer* buffer = client_write_queue_.front();
        client_write_queue_.pop_front();
        delete buffer;
    }
}

int ProxyClient::Update(bool read, uint64_t handle) {
    int length = 0;
    if (!parsed_) {
        length = ParseRequest();
    } else if (read && handle == m_handle) {
        length = Read();
        m_recv_data_len += (length > 0 ? length : 0);
    } else if (!read && handle == m_handle) {
        length = Write();
        if (m_peer_reading_wait && client_write_queue_.empty()) {
            INF("handle %lu, write, peer reading wait, peer read", handle);
            length = PeerRead();
            m_send_data_len += (length > 0 ? length : 0);
        }
    } else if (read && handle == m_peer_handle) {
        length = PeerRead();
        m_send_data_len += (length > 0 ? length : 0);
    } else if (!read && handle == m_peer_handle) {
        length = PeerWrite();
        if (m_reading_wait && remote_write_queue_.empty()) {
            length = Read();
            m_recv_data_len += (length > 0 ? length : 0);
        }
    }
    return length;
}

uint64_t ProxyClient::GetPeerHandle() {
    return m_peer_handle;
}

int ProxyClient::ParseRequest() {
    char buffer[1];
    char last;
    std::stringbuf* buf;
    while (true) {
        int nread = m_netio->Recv(m_handle, reinterpret_cast<char*>(buffer), 1);
        if (nread == 0) {
            return 0;
        } else if (nread == -1) {
            return -1;
        }
        buf = &first_buf_;
        if (*buffer == '\r') {
            continue;
        } else if (*buffer == '\n') {
            if (!first_line_read_) {
                first_line_read_ = true;
                continue;
            } else if (last == '\n') {
                break;
            }
        }
        last = *buffer;
        if (first_line_read_) {
            buf = &parsed_buf_;
        }
        // TRACE("%c", last);
        buf->sputn(buffer, 1);
    }
    // TRACE("\n");
    std::vector<std::string> vec;
    xlib::Split(first_buf_.str(), " ", &vec);
    // DBG("vec size %zu, %s", vec.size(), first_buf_.str().c_str())
    if (vec.size() < 2) {
        return -1;
    }
    std::string host, port, proto;
    if (vec[0] == "CONNECT") {
        std::vector<std::string> two;
        xlib::Split(vec[1], ":", &two);
        if (two.size() != 2) {
            return -1;
        }
        host = two[0];
        port = two[1];
        proto = "https";
    } else {
        std::vector<std::string> third;
        xlib::Split(vec[1], "/", &third);
        // DBG("third size %zu, %s", third.size(), vec[1].c_str())
        if (third.size() < 2) {
            return -1;
        }
        std::vector<std::string> ot;
        xlib::Split(third[1], ":", &ot);
        // DBG("ot size %zu, %s", ot.size(), third[1].c_str())
        if (ot.size() == 1) {
            host = ot[0];
            port = "80";
        } else if (ot.size() == 2) {
            host = ot[0];
            port = ot[1];
        } else {
            return -1;
        }
        proto = "http";
    }

    extern char conf_domain[];
    extern char conf_port[];
    extern bool conf_forward;
    m_remote_host = std::string(host + ":" + port);
    char ip[16];
    std::string used_host = host, used_port = port;
    if (conf_forward) {
        used_host = std::string(conf_domain);
        used_port = std::string(conf_port);
    }
    if (xlib::GetIpByDomain(used_host.c_str(), ip) < 0) {
        return -1;
    }

    char bt[1] = {'\n'};
    first_buf_.sputn(bt, 1);
    parsed_buf_.sputn(bt, 1);
    if ( proto == "https" ) {
        if (conf_forward) {
            remote_write_queue_.push_back(new xlib::Buffer(first_buf_.str().c_str(), first_buf_.str().size()));
            remote_write_queue_.push_back(new xlib::Buffer(parsed_buf_.str().c_str(), parsed_buf_.str().size()));
        } else {
            std::string str = "HTTP/1.1 200 Connection established\r\n\r\n";
            client_write_queue_.push_back(new xlib::Buffer(str.c_str(), str.size()));
        }
    } else {
        remote_write_queue_.push_back(new xlib::Buffer(first_buf_.str().c_str(), first_buf_.str().size()));
        remote_write_queue_.push_back(new xlib::Buffer(parsed_buf_.str().c_str(), parsed_buf_.str().size()));
    }
    DBG("start connect %s,%s", host.c_str(), used_port.c_str());
    m_peer_handle = m_netio->ConnectPeer(ip, std::stoi(used_port));
    if (m_peer_handle <= 0) {
        return -1;
    }
    DBG("connect ok, peer handle, %lu", m_peer_handle);
    extern ProxyServer ps;
    ps.AddPeerClient(m_handle, m_peer_handle);
    parsed_ = true;
    int length = 0;
#if defined(__linux__)
    length = Write();
    DBG("connect write %d", length);
    length = PeerWrite();
    DBG("peer connect write %d", length);
#endif
    return length;
}

int ProxyClient::Read() {
    int length = 0;
    int read_num = 0;
    while (remote_write_queue_.size() < MAX_QUEUE_SIZE) {
        int nread = m_netio->Recv(m_handle, m_read_buffer, sizeof(m_read_buffer-1));
        if (nread == 0) {
            m_reading_wait = false;
            break;
        } else if (nread == -1) {
            DBG("client read error %s,code:%d", strerror(errno), nread);
            return -1;
        }
        length += nread;
        if (nread > 0) {
            read_num++;
            remote_write_queue_.push_back(new xlib::Buffer(m_read_buffer, nread));
            int ret = PeerWrite();
            if (ret <= 0) {
                DBG("reading wait error %s,code:%d", strerror(errno), ret);
                m_reading_wait = true;
                break;
            }
        }
    }
    TRACE("read num %d, wait %d", read_num, m_reading_wait);
    return length;
}

int ProxyClient::Write() {
    int length = 0;
    while (!client_write_queue_.empty()) {
        xlib::Buffer* buffer = client_write_queue_.front();
        int written = m_netio->Send(m_handle, buffer->Bytes(), buffer->Length());
        if (written == 0) {
            break;
        } else if (written == -1) {
            DBG("client write error %s,code:%d", strerror(errno), written);
            return -1;
        }
        buffer->Add(written);
        if (buffer->Length() == 0) {
            client_write_queue_.pop_front();
            delete buffer;
        } else {
            break;
        }
        length += written;
    }
    return length;
}

int ProxyClient::PeerRead() {
    int length = 0;
    int read_num = 0;
    while (client_write_queue_.size() < MAX_QUEUE_SIZE) {
        int nread = m_netio->Recv(m_peer_handle, m_read_buffer, sizeof(m_read_buffer)-1);
        if (nread == 0) {
            m_peer_reading_wait = false;
            break;
        } else if (nread== -1) {
            DBG("remote read error %s,code:%d", strerror(errno), nread);
            return -1;
        }
        length += nread;
        if (nread > 0) {
            read_num++;
            client_write_queue_.push_back(new xlib::Buffer(m_read_buffer, nread));
            int ret = Write();
            if (ret <= 0) {
                DBG("peer reading wait error %s,code:%d", strerror(errno), ret);
                m_peer_reading_wait = true;
                break;
            }
        }
    }
    TRACE("handle %lu, peer read num %d, wait %d",
        m_peer_handle, read_num, m_peer_reading_wait);
    return length;
}

int ProxyClient::PeerWrite() {
    int length = 0;
    while (!remote_write_queue_.empty()) {
        xlib::Buffer* buffer = remote_write_queue_.front();
        int written = m_netio->Send(m_peer_handle, buffer->Bytes(), buffer->Length());
        if (written == 0) {
            break;
        } else if (written == -1) {
            DBG("remote write error %s,code:%d", strerror(errno), written);
            return -1;
        }
        buffer->Add(written);
        if (buffer->Length() == 0) {
            remote_write_queue_.pop_front();
            delete buffer;
        } else {
            break;
        }
        length += written;
    }
    return length;
}
