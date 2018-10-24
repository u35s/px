#include <string.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include "xlib/xlib.h"
#include "xlib/log.h"
#include "pc.h"

ProxyClient::ProxyClient(const uint64_t handle, xlib::NetIO* netio)
    : m_handle(handle), m_peer_handle(0), first_line_read_(false), parsed_(false),
      m_netio(netio) {
}

ProxyClient::~ProxyClient() {
}

void ProxyClient::Update(bool read, uint64_t handle) {
    if (!parsed_) {
        ParseRequest();
    } else if (read && handle == m_handle) {
        DBG("read %d, no peer", read);
        Read();
    } else if (!read && handle == m_handle) {
        DBG("read %d, no peer", read);
        Write();
    } else if (read && handle == m_peer_handle) {
        DBG("read %d, peer", read);
        PeerRead();
    } else if (!read && handle == m_peer_handle) {
        DBG("read %d, peer", read);
        PeerWrite();
    }
}

void ProxyClient::ParseRequest() {
    char buffer[1];
    char last;
    std::stringbuf* buf;
    while (true) {
        int nread = m_netio->Recv(m_handle, (char*)buffer, 1);
        if (nread == 0) {
            // delete this;
            return;
        } else if (nread == -1) {
            if (errno == EAGAIN) {
                return;
            } else {
                // delete this;
                return;
            }
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
        LOG("%c", last);
        buf->sputn(buffer, 1);
    }
    LOG("\n")
    std::vector<std::string> vec;
    xlib::split(first_buf_.str(), " ", vec);
    if (vec.size() < 2) {
        // delete this;
        return;
    }
    std::string host, port, proto;
    if (vec[0] == "CONNECT") {
        std::vector<std::string> two;
        xlib::split(vec[1], ":", two);
        if (two.size() != 2) {
            // delete this;
            return;
        }
        host = two[0];
        port = two[1];
        proto = "https";
    } else {
        std::vector<std::string> third;
        xlib::split(vec[1], "/", third);
        if (third.size() < 3) {
            // delete this;
            return;
        }
        std::vector<std::string> ot;
        xlib::split(third[1], ":", ot);
        if (ot.size() == 1) {
            host = ot[0];
            port = "80";
        } else if (ot.size() == 2) {
            host = ot[0];
            port = ot[1];
        } else {
            // delete this;
            return;
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
    if (xlib::get_ip_by_domain(used_host.c_str(), ip) < 0) {
        // delete this;
        return;
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
    m_peer_handle = m_netio->ConnectPeer(ip, std::stoi(used_port));
    if (m_peer_handle < 0) {
        // delete this;
        return;
    }
    extern ProxyServer ps;
    ps.AddPeerClient(m_handle, m_peer_handle);
    parsed_ = true;
    PeerWrite();
}

bool ProxyClient::Read() {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int nread = m_netio->Recv(m_handle, buffer, sizeof(buffer));
        if (nread == 0) {
            // delete this;
            break;
        } else if (nread == -1) {
            if (errno == EAGAIN) {
                return true;
            } else {
                INF("client read error %s,code:%d", strerror(errno), nread);
                // delete this;
                break;
            }
        }
        remote_write_queue_.push_back(new xlib::Buffer(buffer, nread));
    }
    PeerWrite();
    return true;
}

bool ProxyClient::Write() {
    while (!client_write_queue_.empty()) {
        xlib::Buffer* buffer = client_write_queue_.front();
        int written = m_netio->Send(m_handle, buffer->Bytes(), buffer->Length());
        if (written == -1) {
            if (errno == EAGAIN) {
                return true;
            } else {
                INF("client write error %s,code:%d", strerror(errno), written);
                // delete this;
                return false;
            }
        }
        buffer->Add(written);
        if (buffer->Length() == 0) {
            client_write_queue_.pop_front();
            // delete buffer;
        }
    }
    return true;
}

bool ProxyClient::PeerRead() {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int nread = m_netio->Recv(m_peer_handle, buffer, sizeof(buffer)-1);
        if (nread == 0) {
            // connection closed
            // delete this;
            break;
        } else if (nread== -1) {
            if (errno == EAGAIN) {
                return true;
            }  else {
                INF("remote read error %s,code:%d", strerror(errno), nread);
                // delete this;
                break;
            }
        }
        client_write_queue_.push_back(new xlib::Buffer(buffer, nread));
    }
    Write();
    return true;
}

bool ProxyClient::PeerWrite() {
    while (!remote_write_queue_.empty()) {
        xlib::Buffer* buffer = remote_write_queue_.front();
        int written = m_netio->Send(m_peer_handle, buffer->Bytes(), buffer->Length());
        if (written == -1) {
            if (errno == EAGAIN) {
                return true;
            } else {
                INF("remote write error %s,code:%d", strerror(errno), written);
                // delete this;
                return false;
            }
        }
        buffer->Add(written);
        if (buffer->Length() == 0) {
            remote_write_queue_.pop_front();
            delete buffer;
        }
    }
    return true;
}
