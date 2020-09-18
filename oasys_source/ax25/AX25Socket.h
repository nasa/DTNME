/*
 *    Copyright 2007-2008 Darren Long, darren.long@mac.com
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

#ifndef _OASYS_AX25SOCKET_H_
#define _OASYS_AX25SOCKET_H_

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

// for AX25 support
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

// for oasys integration
#include "../io/IO.h"
#include "../io/IOClient.h"
#include "../compat/inttypes.h"
//#include "../debug/Log.h"


#include "../thread/Thread.h"

namespace oasys {

/*****************************************************************************/
//                  Class AX25Socket Specification
/*****************************************************************************/
class AX25Socket: public Logger, 
                 virtual public IOHandlerBase {

private:
    AX25Socket(const AX25Socket&); ///< Prohibited constructor
    
public:
    AX25Socket(int socktype, const char* logbase);

    AX25Socket(int socktype, int sock,
                const std::string& remote_addr,
                const char* logbase);
             
    virtual ~AX25Socket() { close(); };

    /// Set the socket parameters
    void configure();

    //@{
    /// System call wrappers
    virtual int bind(const std::string& axport, const std::string& local_call);
    virtual int connect(const std::string axport);
    virtual int connect(const std::string axport, const std::string remote_call, 
                        std::vector<std::string>& digi_route);
    virtual int close();
    virtual int shutdown(int how);
    
    virtual int send(const char* bp, size_t len, int flags);
    virtual int sendto(char* bp, size_t len, int flags,
                       const std::string& call, std::vector<std::string>& digi_route);
    virtual int sendmsg(const struct msghdr* msg, int flags);
    
    virtual int recv(char* bp, size_t len, int flags);
    virtual int recvfrom(char* bp, size_t len, int flags,
                         std::string* addr);
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
        INIT,       ///< initial state
        LISTENING,  ///< server socket, called listen()
        CONNECTING, ///< client socket, called connect()
        ESTABLISHED,    ///< connected socket, data can flow
        RDCLOSED,   ///< shutdown(SHUT_RD) called, writing still enabled
        WRCLOSED,   ///< shutdown(SHUT_WR) called, reading still enabled
        CLOSED,     ///< shutdown called for both read and write
        FINI        ///< close() called on the socket
    };
        
    /**
     * Return the current state.
     */
    state_t state() { return state_; }
        
    /**
     * Socket parameters are public fields that should be set after
     * creating the socket but before the socket is used.
     */     
    struct ax25_socket_params {
        ax25_socket_params() :
            ax25_t1_        (100),
            ax25_t2_        (50),
            ax25_t3_        (900),
            ax25_window_    (1),
            ax25_n2_        (10),
            ax25_backoff_   (true),
            reuseaddr_      (true),
            recv_bufsize_   (0),
            send_bufsize_   (0)
        {
            linger_.l_onoff = 0;
            linger_.l_linger = 5;
        }

            
        /////////////////////////////////////////////////////////////////////// 
        // SOL_AX25 Socket Options
        /////////////////////////////////////////////////////////////////////// 
        // How long to wait before retransmitting an unacknowledged frame.
        uint    ax25_t1_;       // t1 timer in 1/10 sec     
        // The minimum amount of time to wait for another
        // frame to be received before transmitting an acknowledgement.
        uint    ax25_t2_;       // t2 timer in 1/10 sec     
        // The period of time we wait between sending a check that the 
        // link is still active.
        uint    ax25_t3_;       // t3 timer     
        // The maximum number of unacknowledged transmitted frames.
        uint    ax25_window_;   // window size      
        // How many times to retransmit a frame before assuming 
        // the connection has failed.
        uint    ax25_n2_;       // retry counter        
        // backoff type for collision avoidance
        bool    ax25_backoff_;  // true = exponential, false = linear
        // DML TODO: enable: Packet length (AX25_PACLEN)
        //uint  ax25_paclen_;   // The AX.25 frame length       
        // DML TODO: enable:  Allow bigger windows (AX25_EXTSEQ)
        //uint  ax25_extseq_;   // Extended sequence numbers

        /////////////////////////////////////////////////////////////////////// 
        // SOL_SOCKET Socket Options
        ///////////////////////////////////////////////////////////////////////                 
        bool reuseaddr_;    // default: on
        int  recv_bufsize_; // default: system setting
        int  send_bufsize_; // default: system setting        
        /*    
        struct linger {
            int l_onoff;    // linger active 
            int l_linger;   // how many seconds to linger for
        };    
        */          
        struct linger linger_;// members as above        
    } params_;



    /// The socket file descriptor
    inline int fd() { return fd_; }
    
    /// The local call that the socket is bound to
    inline std::string local_call();
    
    /// The local axport that the socket is bound or connected with
    inline std::string axport();    
                          
    /// The remote call that the socket is connected to
    inline std::string remote_call();
    
    /// The via route that the connection is routed through
    inline std::vector<std::string> via_route();
                                  
    /// Set the local call that the socket is bound to
    inline void set_local_call(const std::string& call);
    
    /// Set the digiroute that the socket is bound to
    inline void set_via_route(const std::vector<std::string>& route);
    
    /// Set the local axport that the socket is tied to
    inline void set_axport(const std::string& axport);    
                                      
    /// Set the remote call that the socket is connected to
    inline void set_remote_call(const std::string& call);
                                              
    /* 
    /// Wrapper around the logging function needed for abort_on_error
    inline int logf(log_level_t level, const char *fmt, ...) PRINTFLIKE(3, 4);
    */

    /// logfd can be set to false to disable the appending of the
    /// socket file descrAX25tor
    void set_logfd(bool logfd) { logfd_ = logfd; }

    /// Public for use with nonblocking semantics
    void init_socket();
    
protected:
    int     fd_;
    int     socktype_;
    state_t state_;
    bool    logfd_;
    
    std::string local_call_;    ///< local callsign to bind to
    std::string axport_;        ///< local axport to bind or connect with
    std::string remote_call_;   ///< remote call to bind to
    std::vector<std::string> via_route_; ///< source routing info
    
    void set_state(state_t state);
    const char* statetoa(state_t state);
    
    inline void get_local();
    inline void get_remote();
    
    bool bound_;
    
};

std::string
AX25Socket::local_call()
{
    if (local_call_ == "NO_CALL") get_local();
    return local_call_;
}

std::string
AX25Socket::axport()
{
    //if (axport_ == "None") get_local();
    return axport_;
}

std::string
AX25Socket::remote_call()
{
    if (remote_call_ == "NO_CALL") get_remote();
    return remote_call_;
}

std::vector<std::string>
AX25Socket::via_route()
{
	return via_route_;

}

void
AX25Socket::set_via_route(const std::vector<std::string>& route)
{
	via_route_ = route;
}

void
AX25Socket::set_local_call(const std::string& call)
{
    local_call_ = call;
}

void AX25Socket::set_axport(const std::string& axport)
{
    axport_ = axport;
}

void
AX25Socket::set_remote_call(const std::string& call)
{
    remote_call_ = call;
}

void
AX25Socket::get_local()
{
    if (fd_ < 0)
        return;
    
    struct ::full_sockaddr_ax25 sax25;
    socklen_t slen = sizeof sax25;
    if (::getsockname(fd_, (struct sockaddr *)&sax25, &slen) == 0) {
        local_call_ = ax25_ntoa((const ax25_address*)sax25.fsa_ax25.sax25_call.ax25_call);
    }
}

void
AX25Socket::get_remote()
{
    if (fd_ < 0)
        return;
           
    struct full_sockaddr_ax25 sax25;
    socklen_t slen = sizeof sax25;
    if (::getpeername(fd_, (struct sockaddr *)&sax25, &slen) == 0) {
        remote_call_ = ax25_ntoa((const ax25_address*)sax25.fsa_ax25.sax25_call.ax25_call);
    }
}


} // namespace oasys
#endif /* #ifdef OASYS_AX25_ENABLED  */
#endif /* #ifndef _OASYS_AX25SOCKET_H_ */
