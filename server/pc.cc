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
    : m_handle(handle), m_peer_handle(0), first_line_read_(false), parsed_(false),
      m_netio(netio) {
}

ProxyClient::~ProxyClient() {
    m_netio->Close(m_handle);
    m_netio->Close(m_peer_handle);

    while (!remote_write_queue_.empty()) {
        xlib::Buffer* buffer = remote_write_queue_.front();
        delete buffer;
        remote_write_queue_.pop_front();
    }
    while (!client_write_queue_.empty()) {
        xlib::Buffer* buffer = client_write_queue_.front();
        delete buffer;
        client_write_queue_.pop_front();
    }
}

int ProxyClient::Update(bool read, uint64_t handle) {
    int length = 0;
    if (!parsed_) {
        length = ParseRequest();
    } else if (read && handle == m_handle) {
        length = Read();
        // DBG("read %d, no peer, length %d", read, length);
    } else if (!read && handle == m_handle) {
        length = Write();
        // DBG("read %d, no peer, length %d", read, length);
    } else if (read && handle == m_peer_handle) {
        length = PeerRead();
        // DBG("read %d, peer, length %d", read, length);
    } else if (!read && handle == m_peer_handle) {
        length = PeerWrite();
        // DBG("read %d, peer, length %d", read, length);
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
        // LOG("%c", last);
        buf->sputn(buffer, 1);
    }
    // LOG("\n");
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

    extern char* conf_domain;
    extern char* conf_port;
    extern bool conf_forward;

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
    if (m_peer_handle < 0) {
        return -1;
    }
    DBG("connect ok, peer handle, %lu", m_peer_handle);
    extern ProxyServer ps;
    ps.AddPeerClient(m_handle, m_peer_handle);
    parsed_ = true;
    int length = PeerWrite();
    DBG("connect write %d", length)
    return 0;
}

int ProxyClient::Read() {
    char buffer[1024];
    int length;
    while (true) {
        memset(buffer, 0, 1024);
        int nread = m_netio->Recv(m_handle, buffer, sizeof(buffer));
        if (nread == 0) {
            break;
        } else if (nread == -1) {
            INF("client read error %s,code:%d", strerror(errno), nread);
            return -1;
        }
        length += nread;
        remote_write_queue_.push_back(new xlib::Buffer(buffer, nread));
    }
    PeerWrite();
    return length;
}

int ProxyClient::Write() {
    int length;
    while (!client_write_queue_.empty()) {
        xlib::Buffer* buffer = client_write_queue_.front();
        int written = m_netio->Send(m_handle, buffer->Bytes(), buffer->Length());
        if (written == 0) {
            break;
        } else if (written == -1) {
            INF("client write error %s,code:%d", strerror(errno), written);
            return -1;
        }
        buffer->Add(written);
        if (buffer->Length() == 0) {
            client_write_queue_.pop_front();
            // delete buffer;
        } else {
            break;
        }
        length += written;
    }
    return length;
}

int ProxyClient::PeerRead() {
    char buffer[1024];
    int  length;
    while (true) {
        memset(buffer, 0, 1024);
        int nread = m_netio->Recv(m_peer_handle, buffer, sizeof(buffer)-1);
        if (nread == 0) {
            break;
        } else if (nread== -1) {
            INF("remote read error %s,code:%d", strerror(errno), nread);
            return -1;
        }
        length += nread;
        client_write_queue_.push_back(new xlib::Buffer(buffer, nread));
    }
    Write();
    return length;
}

int ProxyClient::PeerWrite() {
    int length;
    while (!remote_write_queue_.empty()) {
        xlib::Buffer* buffer = remote_write_queue_.front();
        int written = m_netio->Send(m_peer_handle, buffer->Bytes(), buffer->Length());
        if (written == 0) {
            break;
        } else if (written == -1) {
            INF("remote write error %s,code:%d", strerror(errno), written);
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
