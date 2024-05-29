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

#ifndef _OASYS_IP_SOCKET_H_
#define _OASYS_IP_SOCKET_H_

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "IO.h"
#include "../compat/inttypes.h"
#include "../debug/Log.h"

namespace oasys {

/**
 * The maximum length of a UDP packet. This isn't really accurate as a
 * maximum payload size since it doesn't take into account the space
 * for the IP header or the UDP header, but is a valid upper bound for
 * the purposes of buffer allocation.
 *
 * XXX/demmer is this in some system header somewhere?
 */
#define MAX_UDP_PACKET 65535

#ifndef INADDR_NONE
#define INADDR_NONE 0
#endif /* INADDR_NONE */

/**
 * \class IPSocket
 *
 * IPSocket is a base class that wraps a network socket. It is a base
 * for TCPClient, TCPServer, and UDPSocket.
 */
class IPSocket : public Logger, 
                 virtual public IOHandlerBase {
private:
    IPSocket(const IPSocket&); ///< Prohibited constructor
    
public:
    // Constructor / destructor
    IPSocket(int socktype, const char* logbase);
    IPSocket(int socktype, int sock,
             in_addr_t remote_addr, u_int16_t remote_port,
             const char* logbase);
    virtual ~IPSocket();

    /// Set the socket parameters
    void configure();

    //@{
    /// System call wrappers
    virtual int bind(in_addr_t local_addr, u_int16_t local_port);
    virtual int connect();
    virtual int connect(in_addr_t remote_addr, u_int16_t remote_port);
    virtual int close();
    virtual int shutdown(int how);
    
    virtual int send(const char* bp, size_t len, int flags);
    virtual int sendto(char* bp, size_t len, int flags,
                       in_addr_t addr, u_int16_t port);
    virtual int sendmsg(const struct msghdr* msg, int flags);
    
    virtual int recv(char* bp, size_t len, int flags);
    virtual int recvfrom(char* bp, size_t len, int flags,
                         in_addr_t *addr, u_int16_t *port);
    virtual int recvmsg(struct msghdr* msg, int flags);

    //@}

    /// In case connect() was called on a nonblocking socket and
    /// returned EINPROGRESS, this fn returns the errno result of the
    /// connect attempt. It also sets the socket state appropriately
    int async_connect_result();
    
    /// Wrapper around poll() for this socket's fd
    virtual int poll_sockfd(int events, int* revents, int timeout_ms);
    
    /// Socket State values
    enum state_t {
        INIT, 		///< initial state
        LISTENING,	///< server socket, called listen()
        CONNECTING,	///< client socket, called connect()
        ESTABLISHED, 	///< connected socket, data can flow
        RDCLOSED,	///< shutdown(SHUT_RD) called, writing still enabled
        WRCLOSED,	///< shutdown(SHUT_WR) called, reading still enabled
        CLOSED,		///< shutdown called for both read and write
        FINI		///< close() called on the socket
    };
        
    /**
     * Return the current state.
     */
    state_t state() { return state_; }
        
    /**
     * Socket parameters are public fields that should be set after
     * creating the socket but before the socket is used.
     */
    struct ip_socket_params {
        ip_socket_params() :
            reuseaddr_    (true),
            reuseport_    (false),
            tcp_nodelay_  (false),
            broadcast_    (false),
            multicast_    (false),
            mcast_ttl_    (1),
            recv_bufsize_ (0),
            send_bufsize_ (0)
        {
        }
        
        bool reuseaddr_;	// default: on
        bool reuseport_;	// default: off
        bool tcp_nodelay_;	// default: off
        bool broadcast_;	// default: off
        bool multicast_;    // default: off

        u_int mcast_ttl_;   // default: 1

        int recv_bufsize_;	// default: system setting
        int send_bufsize_;	// default: system setting
    } params_;
    
    /// The socket file descriptor
    inline int fd() { return fd_; }
    
    /// The local address that the socket is bound to
    inline in_addr_t local_addr();
                
    /// The local port that the socket is bound to
    inline u_int16_t local_port();
                          
    /// The remote address that the socket is connected to
    inline in_addr_t remote_addr();
                              
    /// The remote port that the socket is connected to
    inline u_int16_t remote_port();
                                  
    /// Set the local address that the socket is bound to
    inline void set_local_addr(in_addr_t addr);
                                      
    /// Set the local port that the socket is bound to
    inline void set_local_port(u_int16_t port);
                                          
    /// Set the remote address that the socket is connected to
    inline void set_remote_addr(in_addr_t addr);
                                              
    /// Set the remote port that the socket is connected to
    inline void set_remote_port(u_int16_t port);
                                                  
    /* 
    /// Wrapper around the logging function needed for abort_on_error
    inline int logf(log_level_t level, const char *fmt, ...) PRINTFLIKE(3, 4);
    */

    /// logfd can be set to false to disable the appending of the
    /// socket file descriptor
    void set_logfd(bool logfd) { logfd_ = logfd; }

    /// Public for use with nonblocking semantics
    void init_socket();
    
protected:
    int     fd_;
    int     socktype_;
    state_t state_;
    bool    logfd_;
    
    in_addr_t local_addr_;
    u_int16_t local_port_;
    in_addr_t remote_addr_;
    u_int16_t remote_port_;
    
    void set_state(state_t state);
    const char* statetoa(state_t state);
    
    inline void get_local();
    inline void get_remote();
    
};

in_addr_t
IPSocket::local_addr()
{
    if (local_addr_ == INADDR_NONE) get_local();
    return local_addr_;
}

u_int16_t
IPSocket::local_port()
{
    if (local_port_ == 0) get_local();
    return local_port_;
}

in_addr_t
IPSocket::remote_addr()
{
    if (remote_addr_ == INADDR_NONE) get_remote();
    return remote_addr_;
}

u_int16_t
IPSocket::remote_port()
{
    if (remote_port_ == 0) get_remote();
    return remote_port_;
}

void
IPSocket::set_local_addr(in_addr_t addr)
{
    local_addr_ = addr;
}

void
IPSocket::set_local_port(u_int16_t port)
{
    local_port_ = port;
}

void
IPSocket::set_remote_addr(in_addr_t addr)
{
    remote_addr_ = addr;
}

void
IPSocket::set_remote_port(u_int16_t port)
{
    remote_port_ = port;
}

void
IPSocket::get_local()
{
    if (fd_ < 0)
        return;
    
    struct sockaddr_in sin;
    socklen_t slen = sizeof sin;
    if (::getsockname(fd_, (struct sockaddr *)&sin, &slen) == 0) {
        local_addr_ = sin.sin_addr.s_addr;
        local_port_ = ntohs(sin.sin_port);
    }
}

void
IPSocket::get_remote()
{
    if (fd_ < 0)
        return;
           
    struct sockaddr_in sin;
    socklen_t slen = sizeof sin;
    if (::getpeername(fd_, (struct sockaddr *)&sin, &slen) == 0) {
        remote_addr_ = sin.sin_addr.s_addr;
        remote_port_ = ntohs(sin.sin_port);
    }
}

} // namespace oasys
 
#endif /* _OASYS_IP_SOCKET_H_ */
