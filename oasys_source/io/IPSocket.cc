/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "IPSocket.h"
#include "NetUtils.h"
#include "debug/Log.h"
#include "debug/DebugUtils.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace oasys {

IPSocket::IPSocket(int socktype, const char* logbase)
    : Logger("IPSocket", "%s", logbase)
{
    state_       = INIT;
    local_addr_  = INADDR_ANY;
    local_port_  = 0;
    remote_addr_ = INADDR_NONE;
    remote_port_ = 0;
    fd_          = -1;
    socktype_    = socktype;
    logfd_       = true;
}

IPSocket::IPSocket(int socktype, int sock,
                   in_addr_t remote_addr, u_int16_t remote_port,
                   const char* logbase)
    : Logger("IPSocket", "%s/%d", logbase, sock)
{
    fd_       = sock;
    socktype_ = socktype;
    
    state_       = ESTABLISHED;
    local_addr_  = INADDR_NONE;
    local_port_  = 0;
    remote_addr_ = remote_addr;
    remote_port_ = remote_port;
    logfd_       = false;
    
    configure();
}

IPSocket::~IPSocket()
{
    close();
}

void
IPSocket::init_socket()
{
    // should only be called at real init time or after a call to close()
    ASSERT(state_ == INIT || state_ == FINI);
    ASSERT(fd_ == -1);
    state_ = INIT;
    
    fd_ = socket(PF_INET, socktype_, 0);
    if (fd_ == -1) {
        logf(LOG_ERR, "error creating socket: %s", strerror(errno));
        return;
    }

    if (logfd_)
        Logger::logpath_appendf("/%d", fd_);
    
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "created socket %d", fd_);
#endif
    
    configure();
}

const char*
IPSocket::statetoa(state_t state)
{
    switch (state) {
    case INIT:          return "INIT";
    case LISTENING:     return "LISTENING";
    case CONNECTING:    return "CONNECTING";
    case ESTABLISHED:   return "ESTABLISHED";
    case RDCLOSED:      return "RDCLOSED";
    case WRCLOSED:      return "WRCLOSED";
    case CLOSED:        return "CLOSED";
    case FINI:          return "FINI";
    }
    ASSERT(0);
    return NULL;
}

void
IPSocket::set_state(state_t state)
{
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "state %s -> %s", statetoa(state_), statetoa(state));
#endif
    state_ = state;
}

int
IPSocket::bind(in_addr_t local_addr, u_int16_t local_port)
{
    struct sockaddr_in sa;

    if (fd_ == -1) init_socket();

    local_addr_ = local_addr;
    local_port_ = local_port;

#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "binding to %s:%d", intoa(local_addr), local_port);
#endif

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = local_addr_;
    sa.sin_port = htons(local_port_);
    if (::bind(fd_, (struct sockaddr*) &sa, sizeof(sa)) != 0) {
        int err = errno;
        logf(LOG_ERR, "error binding to %s:%d: %s",
             intoa(local_addr_), local_port_, strerror(err));
        return -1;
    }

    return 0;
}

int
IPSocket::connect()
{
    struct sockaddr_in sa;

    if (ESTABLISHED == state_)
        return 0;

    if (fd_ == -1) init_socket();

    log_debug("connecting to %s:%d", intoa(remote_addr_), remote_port_);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = remote_addr_;
    sa.sin_port = htons(remote_port_);

    set_state(CONNECTING);

    if (::connect(fd_, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        if (errno == EISCONN)
            log_debug("already connected to %s:%d",
                      intoa(remote_addr_), remote_port_);
        else if (errno == EINPROGRESS) {
            log_debug("delayed connect to %s:%d (EINPROGRESS)",
                      intoa(remote_addr_), remote_port_);
        } else {
            log_debug("error connecting to %s:%d: %s",
                      intoa(remote_addr_), remote_port_, strerror(errno));
        }
        
        return -1;
    }

    set_state(ESTABLISHED);

    return 0;
}

int
IPSocket::async_connect_result()
{
    ASSERT(state_ == CONNECTING);

    int result;
    socklen_t len = sizeof(result);
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "getting connect result");
#endif
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &result, &len) != 0) {
        logf(LOG_ERR, "error getting connect result: %s", strerror(errno));
        return errno;
    }

    if (result == 0) {
        set_state(ESTABLISHED);
    }

    return result;
}

int
IPSocket::connect(in_addr_t remote_addr, u_int16_t remote_port)
{
    remote_addr_ = remote_addr;
    remote_port_ = remote_port;

    return connect();
}

void
IPSocket::configure()
{
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "IPSocket::configure");
    logf(LOG_DEBUG, "    params.reuseaddr_: %d", params_.reuseaddr_);
    logf(LOG_DEBUG, "    params.reuseport_: %d", params_.reuseport_);
    logf(LOG_DEBUG, "    params.broadcast_: %d", params_.broadcast_);
    logf(LOG_DEBUG, "    params.multicast_: %d", params_.multicast_);
    logf(LOG_DEBUG, "    local_addr_      : %d", local_addr_);
    logf(LOG_DEBUG, "    remote_addr_     : %d", remote_addr_);
    logf(LOG_DEBUG, "    local_port_      : %d", local_port_);
    logf(LOG_DEBUG, "    remote_port_     : %d", remote_port_);
#endif

    if (params_.reuseaddr_) {
        int y = 1;
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(LOG_DEBUG, "setting SO_REUSEADDR");
#endif
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting SO_REUSEADDR: %s",
                 strerror(errno));
        }
    }

    if (params_.reuseport_) {
#ifdef SO_REUSEPORT
        int y = 1;
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(LOG_DEBUG, "setting SO_REUSEPORT");
#endif
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting SO_REUSEPORT: %s",
                 strerror(errno));
        }
#else
        logf(LOG_WARN, "error setting SO_REUSEPORT: not implemented");
#endif
    }
    
    if (socktype_ == SOCK_STREAM && params_.tcp_nodelay_) {
        int y = 1;
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(LOG_DEBUG, "setting TCP_NODELAY");
#endif
        if (::setsockopt(fd_, IPPROTO_IP, TCP_NODELAY, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting TCP_NODELAY: %s",
                 strerror(errno));
        }
    }

    if (socktype_ == SOCK_DGRAM && params_.broadcast_) {
        int y = 1;
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(LOG_DEBUG, "setting SO_BROADCAST");
#endif

        if (::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &y, sizeof(y)) != 0) {
            logf(LOG_WARN, "error setting SO_BROADCAST: %s",
                 strerror(errno));
        }
    }
    
    if (socktype_ == SOCK_DGRAM && params_.multicast_) {

        // set up receiver to join in on multicast tree
        struct ip_mreq mcast_request;
        memset(&mcast_request,0,sizeof(struct ip_mreq));

        // Force remote addr to match multicast (class D)
        // 224.0.0.0 - 239.255.255.255
        in_addr_t mcast_addr = inet_addr("224.0.0.0");
        if ((mcast_addr & remote_addr_) != mcast_addr) {
            logf(LOG_WARN, "multicast option set on non-multicast address: "
                           "%s",intoa(remote_addr_));
            return;
        }

        // set up remote address for multicast options struct
        mcast_request.imr_multiaddr.s_addr = remote_addr_;

        // set up local address for multicast options struct
        mcast_request.imr_interface.s_addr = local_addr_;

        // pass struct into setsockopt
        if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         (void*) &mcast_request, sizeof (struct ip_mreq)) < 0)
        {
            logf(LOG_WARN, "error setting multicast options: %s",
                           strerror(errno));
            logf(LOG_WARN, "local_addr_   : %s", intoa(local_addr_));
            logf(LOG_WARN, "remote_addr_  : %s", intoa(remote_addr_));
        }

        // set TTL on outbound packets
        u_char ttl = (u_char) params_.mcast_ttl_ & 0xff;
        if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &ttl, 1)
                < 0) {
            logf(LOG_WARN, "error setting multicast ttl: %s",
                           strerror(errno));
        }

        // restrict outbound multicast to named interface
        // (INADDR_ANY means outbound on all interaces)
        struct in_addr which;
        memset(&which,0,sizeof(struct in_addr));
        which.s_addr = local_addr_;
        if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, &which,
                    sizeof(which)) < 0)
        {
            logf(LOG_WARN, "error setting outbound multicast interface: %s",
                    intoa(local_addr_));
        }
    }

    if (params_.recv_bufsize_ > 0) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(LOG_DEBUG, "setting SO_RCVBUF to %d",
             params_.recv_bufsize_);
#endif
        
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF,
                         &params_.recv_bufsize_,
                         sizeof (params_.recv_bufsize_)) < 0)
        {
            logf(LOG_WARN, "error setting SO_RCVBUF to %d: %s",
                 params_.recv_bufsize_, strerror(errno));
        }
    }
    
    if (params_.send_bufsize_ > 0) {
        logf(LOG_WARN, "setting SO_SNDBUF to %d",
             params_.send_bufsize_);
        
        if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF,
                         &params_.send_bufsize_,
                         sizeof params_.send_bufsize_) < 0)
        {
            logf(LOG_WARN, "error setting SO_SNDBUF to %d: %s",
                 params_.send_bufsize_, strerror(errno));
        }
    }
}
    
int
IPSocket::close()
{
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "closing socket in state %s", statetoa(state_));
#endif

    if (fd_ == -1) {
        ASSERT(state_ == INIT || state_ == FINI);
        return 0;
    }
    
    if (::close(fd_) != 0) {
        logf(LOG_ERR, "error closing socket in state %s: %s",
             statetoa(state_), strerror(errno));
        return -1;
    }
    
    set_state(FINI);
    fd_ = -1;
    return 0;
}

int
IPSocket::shutdown(int how)
{
    const char* howstr;

    switch (how) {
    case SHUT_RD:   howstr = "R";  break;
    case SHUT_WR:   howstr = "W";  break;
    case SHUT_RDWR: howstr = "RW"; break;
        
    default:
        logf(LOG_ERR, "shutdown invalid mode %d", how);
        return -1;
    }
      
#ifdef OASYS_LOG_DEBUG_ENABLED
    logf(LOG_DEBUG, "shutdown(%s) state %s", howstr, statetoa(state_));
#endif
    
    if (state_ == INIT || state_ == FINI) {
        ASSERT(fd_ == -1);
        return 0;
    }
    
    if (::shutdown(fd_, how) != 0) {
        logf(LOG_ERR, "error in shutdown(%s) state %s: %s",
             howstr, statetoa(state_), strerror(errno));
    }
    
    if (state_ == ESTABLISHED) {
        if (how == SHUT_RD)     { set_state(RDCLOSED); }
        if (how == SHUT_WR)     { set_state(WRCLOSED); }
        if (how == SHUT_RDWR)   { set_state(CLOSED); }
        
    } else if (state_ == RDCLOSED && how == SHUT_WR) {
        set_state(CLOSED);
        
    } else if (state_ == WRCLOSED && how == SHUT_RD) {
        set_state(CLOSED);

    } else {
        logf(LOG_ERR, "invalid state %s for shutdown(%s)",
             statetoa(state_), howstr);
        return -1;
    }

    return 0;
}

int
IPSocket::send(const char* bp, size_t len, int flags)
{
    return IO::send(fd_, bp, len, flags, get_notifier(), logpath_);
}

int
IPSocket::sendto(char* bp, size_t len, int flags,
                 in_addr_t addr, u_int16_t port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = addr;
    sa.sin_port = htons(port);

    return IO::sendto(fd_, bp, len, flags, (sockaddr*)&sa, 
                      sizeof(sa), get_notifier(), logpath_);
}

int
IPSocket::sendmsg(const struct msghdr* msg, int flags)
{
    return IO::sendmsg(fd_, msg, flags, get_notifier(), logpath_);
}

int
IPSocket::recv(char* bp, size_t len, int flags)
{
    return IO::recv(fd_, bp, len, flags, 
                    get_notifier(), logpath_);
}

int
IPSocket::recvfrom(char* bp, size_t len, int flags,
                   in_addr_t *addr, u_int16_t *port)
{
    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    
    int cc = IO::recvfrom(fd_, bp, len, flags, (sockaddr*)&sa, &sl, 
                          get_notifier(), logpath_);
    
    if (cc < 0) {
        if (cc != IOINTR)
            logf(LOG_ERR, "error in recvfrom(): %s", strerror(errno));
        return cc;
    }

    if (addr)
        *addr = sa.sin_addr.s_addr;

    if (port)
        *port = htons(sa.sin_port);

    return cc;
}

int
IPSocket::recvmsg(struct msghdr* msg, int flags)
{
    return IO::recvmsg(fd_, msg, flags, get_notifier(), logpath_);
}

int
IPSocket::poll_sockfd(int events, int* revents, int timeout_ms)
{
    short s_events = events;
    short s_revents;
    
    int cc = IO::poll_single(fd_, s_events, &s_revents, timeout_ms, 
                             get_notifier(), logpath_);
    
    if (revents != 0) {
        *revents = s_revents;
    }

    return cc;
}

} // namespace oasys
