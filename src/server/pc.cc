/*
 * Copyright [2018] <Copyright u35s>
 */

#include <string.h>
#include <errno.h>
#include <sstream>
#include <vector>
#include "xlib/string.h"
#include "xlib/conv.h"
#include "xlib/log.h"
#include "xlib/base64.h"
#include "server/ps.h"
#include "server/pc.h"

ProxyClient::ProxyClient(const uint64_t handle, xlib::NetIO* netio)
    : m_buffer(new xlib::Buffer(1<<12)), m_peer_buffer(new xlib::Buffer(1<<12)),
      m_reading_wait(false), m_peer_reading_wait(false),
      m_handle(handle), m_peer_handle(0),
      m_recv_data_len(0), m_send_data_len(0), m_first_line_read(false), m_parsed(false),
      m_netio(netio) {
}

ProxyClient::~ProxyClient() {
    const xlib::SocketInfo* socket_info = m_netio->GetSocketInfo(m_handle);

    INF("client ip " NETADDR_IP_PRINT_FMT ", remote host %s, recv %d, send %d,",
        NETADDR_IP_PRINT_CTX(socket_info), m_remote_host.c_str(),
        m_recv_data_len, m_send_data_len);
    m_netio->Close(m_handle);
    m_netio->Close(m_peer_handle);
    delete m_buffer;
    delete m_peer_buffer;
}

int ProxyClient::Update(bool read, uint64_t handle) {
    int length = 0;
    if (!m_parsed) {
        length = ParseRequest();
    } else if (read && handle == m_handle) {
        length = Read();
        m_recv_data_len += (length > 0 ? length : 0);
    } else if (!read && handle == m_handle) {
        length = Write();
        if (m_peer_reading_wait && m_peer_buffer->Size() == 0) {
            TRACE("handle %lu, write, peer reading wait, peer read", handle);
            length = PeerRead();
            m_send_data_len += (length > 0 ? length : 0);
        }
    } else if (read && handle == m_peer_handle) {
        length = PeerRead();
        m_send_data_len += (length > 0 ? length : 0);
    } else if (!read && handle == m_peer_handle) {
        length = PeerWrite();
        if (m_reading_wait && m_buffer->Size() == 0) {
            length = Read();
            m_recv_data_len += (length > 0 ? length : 0);
        }
    }
    return length;
}

uint64_t ProxyClient::GetPeerHandle() {
    return m_peer_handle;
}

bool ProxyClient::VerifyAuthorization() {
    std::vector<std::string> vec;
    std::string flag("Proxy-Authorization: Basic ");
    xlib::Split(m_parse_buf, "\n", &vec);
    for (size_t i = 0; i < vec.size(); ++i) {
        std::size_t found = vec[i].find(flag);
        if (found != std::string::npos) {
            m_authorization = xlib::Base64Decode(vec[i].substr(flag.size()));
        }
    }
    const Options* options = ProxyServer::Instance().GetOptions();
    bool  pass =  options->auth == m_authorization;
    XDBG("authorization:##,##", m_authorization, pass);
    return pass;
}

int ProxyClient::ParseRequest() {
    char buffer[1];
    char last;
    std::string* buf;
    while (true) {
        int n = m_netio->Recv(m_handle, reinterpret_cast<char*>(buffer), 1);
        if (n == 0) {
            return 0;
        } else if (n == -1) {
            return -1;
        }
        buf = &m_first_line_buf;
        if (*buffer == '\r') {
            continue;
        } else if (*buffer == '\n') {
            if (!m_first_line_read) {
                m_first_line_read = true;
                continue;
            } else if (last == '\n') {
                break;
            }
        }
        last = *buffer;
        if (m_first_line_read) {
            buf = &m_parse_buf;
        }
        buf->append(1, last);
    }
    if (!VerifyAuthorization()) {
        return -1;
    }
    std::vector<std::string> vec;
    xlib::Split(m_first_line_buf, " ", &vec);
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
        if (third.size() < 2) {
            return -1;
        }
        std::vector<std::string> ot;
        xlib::Split(third[1], ":", &ot);
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

    m_remote_host = std::string(host + ":" + port);
    char ip[16];
    std::string used_host = host, used_port = port;
    const Options* options = ProxyServer::Instance().GetOptions();
    // options = NULL; // auto 指针常量失效
    if (options->forward) {
        used_host = options->forward_domain;
        used_port = options->forward_port;
    }
    if (xlib::GetIpByDomain(used_host.c_str(), ip) < 0) {
        return -1;
    }

    m_first_line_buf.append(1, '\n');
    m_parse_buf.append(1, '\n');

    if ( proto == "https" ) {
        if (options->forward) {
            m_buffer->Write(m_first_line_buf.c_str());
            m_buffer->Write(m_parse_buf.c_str());
        } else {
            std::string str = "HTTP/1.1 200 Connection established\r\n\r\n";
            m_peer_buffer->Write(str.c_str());
        }
    } else {
       m_buffer->Write(m_first_line_buf.c_str());
       m_buffer->Write(m_parse_buf.c_str());
    }
    XDBG("start connect ##,##", host, used_port);
    m_peer_handle = m_netio->ConnectPeer(ip, xlib::Stoi(used_port));
    if (m_peer_handle <= 0) {
        return -1;
    }
    DBG("connect ok, peer handle, %lu", m_peer_handle);
    ProxyServer::Instance().AddPeerClient(m_handle, m_peer_handle);
    m_parsed = true;
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
    while (m_buffer->Cap() > 0) {
        int n = m_netio->Recv(m_handle, m_buffer->Next(), m_buffer->Cap());
        if (n == 0) {
            m_reading_wait = false;
            break;
        } else if (n < 0) {
            DBG("client read error %s,code:%d", strerror(errno), n);
            return -1;
        } else if (n > 0) {
            length += n;
            read_num++;
            m_buffer->WriteN(n);
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
    if (m_peer_buffer->Size() <= 0) {
        return 0;
    }
    int written = m_netio->Send(m_handle, m_peer_buffer->Bytes(), m_peer_buffer->Size());
    if (written == 0) {
        return 0;
    } else if (written == -1) {
        DBG("client write error %s,code:%d", strerror(errno), written);
        return -1;
    }
    m_peer_buffer->ReadN(written);
    return written;
}

int ProxyClient::PeerRead() {
    int length = 0;
    int read_num = 0;
    while (m_peer_buffer->Cap() > 0) {
        int n = m_netio->Recv(m_peer_handle, m_peer_buffer->Next(), m_peer_buffer->Cap());
        if (n == 0) {
            m_peer_reading_wait = false;
            break;
        } else if (n < 0) {
            DBG("remote read error %s,code:%d", strerror(errno), n);
            return -1;
        } else if (n > 0) {
            length += n;
            read_num++;
            m_peer_buffer->WriteN(n);
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
    if (m_buffer->Size() <= 0) {
        return 0;
    }
    int written = m_netio->Send(m_peer_handle, m_buffer->Bytes(), m_buffer->Size());
    if (written == 0) {
        return 0;
    } else if (written == -1) {
        DBG("remote write error %s,code:%d", strerror(errno), written);
        return -1;
    }
    m_buffer->ReadN(written);
    return written;
}
