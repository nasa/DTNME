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
#include <arpa/inet.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

#include "io/NetUtils.h"
#include "util/Random.h"
#include "debug/Log.h"
#include "debug/DebugUtils.h"

#include "AX25Socket.h"

namespace oasys {

/*****************************************************************************/
//                  Class AX25Socket Implementation
/*****************************************************************************/
AX25Socket::AX25Socket(int socktype, const char* logbase)
    : Logger("AX25Socket", logbase)
{
    state_       = INIT;
    local_call_  = "NO_CALL";
    remote_call_ = "NO_CALL";
    axport_      = "None";
    fd_          = -1;
    socktype_    = socktype;
    logfd_       = true;
    bound_ = false;
}

AX25Socket::AX25Socket(int socktype, int sock,
                   const std::string& remote_call,
                   const char* logbase)
    : Logger("AX25Socket", "%s/%d", logbase, sock)
{
    fd_       = sock;
    socktype_ = socktype;    
    state_       = ESTABLISHED;
    local_call_  = "NO_CALL";
    remote_call_ = remote_call;
    axport_      = "None";    
    bound_ = false;
    
    configure();
}

void
AX25Socket::init_socket()
{
    // should only be called at real init time or after a call to close()
    ASSERT(state_ == INIT || state_ == FINI);
    ASSERT(fd_ == -1);
    state_ = INIT;
    
    fd_ = socket(AF_AX25, socktype_, 0);
    if (fd_ == -1) {
        logf(LOG_ERR, "error creating socket: %s", strerror(errno));
        return;
    }

    if (logfd_)
        Logger::logpath_appendf("/%d", fd_);
    
    logf(LOG_DEBUG, "created socket %d", fd_);
    
    configure();
}

const char*
AX25Socket::statetoa(state_t state)
{
    switch (state) {
    case INIT:      return "INIT";
    case LISTENING:     return "LISTENING";
    case CONNECTING:    return "CONNECTING";
    case ESTABLISHED:   return "ESTABLISHED";
    case RDCLOSED:  return "RDCLOSED";
    case WRCLOSED:  return "WRCLOSED";
    case CLOSED:    return "CLOSED";
    case FINI:      return "FINI";
    }
    ASSERT(0);
    return NULL;
}

void
AX25Socket::set_state(state_t state)
{
    logf(LOG_DEBUG, "state %s -> %s", statetoa(state_), statetoa(state));
    state_ = state;
}

int
AX25Socket::bind(const std::string& axport, const std::string& local_call)
{
    
    if (fd_ == -1) init_socket();

    local_call_ = local_call;
    axport_ = axport;
    
    struct full_sockaddr_ax25 sax25;
    
    memset(&sax25, 0, sizeof(sax25));
    sax25.fsa_ax25.sax25_family = AF_AX25;
   
    sax25.fsa_ax25.sax25_ndigis = 1;

    static bool once = false;

    if(!once) {
        // loading the axports file
        if (ax25_config_load_ports() == 0) {
            logf(LOG_ERR,"ERROR: problem with axports file");
            return -1;
        }
        once = true;
    }
        
    char* addr;
    
    // lookup the axport name and get the local call associated with it
    if ((addr = 
            ax25_config_get_addr(const_cast<char*>(axport.c_str()))) == NULL) {
        logf(LOG_ERR,"ERROR: invalid AX.25 port name - %s",axport.c_str());
        return -1;
    }
    
    // configure the callsign associated with the local axport
    if (ax25_aton_entry(addr, sax25.fsa_digipeater[0].ax25_call) == -1) {
        logf(LOG_ERR,"ERROR: invalid AX.25 port callsign - %s",axport.c_str());
        return -1;
    }
    

    
    // format the address for the listening socket
    if (ax25_aton_entry(local_call_.c_str(), 
        sax25.fsa_ax25.sax25_call.ax25_call) == -1) {
        logf(LOG_ERR,"ERROR: invalid callsign - %s",local_call.c_str());
        return -1;
    }
        

    logf(LOG_DEBUG, "binding to call %s axport=%s", 
         local_call.c_str(),axport.c_str());


    
    if (ax25_aton_entry(local_call.c_str(), 
        sax25.fsa_ax25.sax25_call.ax25_call) == -1) {
        logf(LOG_ERR, "invalid callsign -  %s",local_call.c_str());
        return -1;
    }
    
    //TODO: DML digi list ???
    
    
    if (::bind(fd_, (struct sockaddr*) &sax25, sizeof(sax25)) != 0) {
        int err = errno;
        logf(LOG_ERR, "error binding to call %s: axport=%s: %s",
             local_call.c_str(), axport.c_str(),strerror(err));
        return -1;
    }
    bound_ = true;
    return 0;
}

int
AX25Socket::connect(std::string axport)
{
    if(false == bound_)
        bind(axport,local_call_);

    if (ESTABLISHED == state_)
    return 0;

    if (fd_ == -1) init_socket();

    log_debug("connecting  to  %s axport=%s", 
              remote_call_.c_str(),axport.c_str());

    struct full_sockaddr_ax25 sax25;
    memset(&sax25, 0, sizeof(sax25));
    sax25.fsa_ax25.sax25_family = AF_AX25;
    sax25.fsa_ax25.sax25_ndigis = 1;

    
    char* addr;
    
    // lookup the axport name and get the local call associated with it
    if ((addr = 
            ax25_config_get_addr(const_cast<char*>(axport.c_str()))) == NULL) {
        logf(LOG_ERR,"ERROR: invalid AX.25 port name - %s",axport.c_str());
        return -1;
    }
    
    // configure the callsign associated with the local axport
    if (ax25_aton_entry(addr, sax25.fsa_digipeater[0].ax25_call) == -1) {
        logf(LOG_ERR,"ERROR: invalid AX.25 port callsign - %s",axport.c_str());
        return -1;
    }
    
    
    // set up a source-route to the destination address
    // TODO - quick hack implemented here to store a single digipeater.  Fix me.
    const char* route[3];
    route[0] = remote_call_.c_str();
    if(0 == via_route_.size() )
    {
    	route[1] = 0;
    }
    else
    {
    	route[1] = via_route_[0].c_str();
    }
    route[2] = 0;
    
    // format the source-route to the destination address
    if (ax25_aton_arglist((const char**)route, &sax25) == -1) {
        logf(LOG_ERR, "invalid destination callsign or digipeater");
        return -1;
    }

    set_state(CONNECTING);

    if (::connect(fd_, (struct sockaddr*)&sax25, sizeof(sax25)) < 0) {
        if (errno == EISCONN) {     
                log_debug("already connected to %s",
                          remote_call_.c_str());
        } 
        else if (errno == EINPROGRESS) {
            log_debug("delayed connect to %s (EINPROGRESS)",
                      remote_call_.c_str());
        } 
	else {
            log_debug("error connecting to %s: %s",
                      remote_call_.c_str(), strerror(errno));
        }
            
        return -1;
    }

    set_state(ESTABLISHED);

    return 0;
}

int
AX25Socket::async_connect_result()
{
    ASSERT(state_ == CONNECTING);

    int result;
    socklen_t len = sizeof(result);
    logf(LOG_DEBUG, "getting connect result");
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
AX25Socket::connect(std::string axport, std::string remote_call,
                    std::vector<std::string>& digi_route)
{
    remote_call_ = remote_call;
    via_route_ = digi_route;

    return connect(axport);
}

void
AX25Socket::configure()
{
    if (params_.ax25_t1_ > 0) {
        logf(LOG_DEBUG, "setting AX25_T1");
        if (::setsockopt(fd_, SOL_AX25, AX25_T1, &params_.ax25_t1_,
                                            sizeof params_.ax25_t1_) != 0) {
            logf(LOG_WARN, "error setting AX25_T1: %s", strerror(errno));
        }
    }
    
    if (params_.ax25_t2_ > 0) {
        logf(LOG_DEBUG, "setting AX25_T2");
        if (::setsockopt(fd_, SOL_AX25, AX25_T2, &params_.ax25_t2_,
                                          sizeof params_.ax25_t2_) != 0) {
            logf(LOG_WARN, "error setting AX25_T2: %s",  strerror(errno));
        }
    }

    if (params_.ax25_t3_ > 0) {
        logf(LOG_DEBUG, "setting AX25_T3");
        if (::setsockopt(fd_, SOL_AX25, AX25_T3, &params_.ax25_t3_,
                                            sizeof params_.ax25_t3_) != 0) {
            logf(LOG_WARN, "error setting AX25_T3: %s", strerror(errno));
        }
    }
    
    if (params_.ax25_n2_ > 0) {
        logf(LOG_DEBUG, "setting AX25_N2");
        if (::setsockopt(fd_, SOL_AX25, AX25_N2, &params_.ax25_n2_, 
                                            sizeof params_.ax25_n2_) != 0) {
            logf(LOG_WARN, "error setting AX25_N2: %s", strerror(errno));
        }
    }  
    
    if (params_.ax25_window_ > 0) {
        logf(LOG_DEBUG, "setting AX25_WINDOW");
        if (::setsockopt(fd_, SOL_AX25, AX25_WINDOW, &params_.ax25_window_,
                                        sizeof params_.ax25_window_) != 0) {
            logf(LOG_WARN, "error setting AX25_WINDOW: %s", strerror(errno));
        }
    }      
    
    if (params_.ax25_backoff_) {
        int y = 1;
        logf(LOG_DEBUG, "setting AX25_BACKOFF");
        if (::setsockopt(fd_, SOL_AX25, AX25_BACKOFF, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting AX25_BACKOFF: %s", strerror(errno));
        }
    }

    if (params_.reuseaddr_) {
        int y = 1;
        logf(LOG_DEBUG, "setting SO_REUSEADDR");
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting SO_REUSEADDR: %s", strerror(errno));
        }
    }
    
    if (params_.recv_bufsize_ > 0) {
        logf(LOG_DEBUG, "setting SO_RCVBUF to %d", params_.recv_bufsize_);
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &params_.recv_bufsize_,
                                       sizeof (params_.recv_bufsize_)) < 0) {
            logf(LOG_WARN, "error setting SO_RCVBUF to %d: %s",
                params_.recv_bufsize_, strerror(errno));
        }
    }
    
    if (params_.send_bufsize_ > 0) {
        logf(LOG_WARN, "setting SO_SNDBUF to %d", params_.send_bufsize_);
        if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &params_.send_bufsize_,
                                        sizeof params_.send_bufsize_) < 0) {
            logf(LOG_WARN, "error setting SO_SNDBUF to %d: %s",
                 params_.send_bufsize_, strerror(errno));
        }
    }    

    logf(LOG_DEBUG, "setting SO_LINGER to %s", 
          (params_.linger_.l_onoff == true)?"TRUE":"FALSE");
    if (::setsockopt(fd_, SOL_SOCKET, SO_LINGER, &params_.linger_,
                                            sizeof(struct linger)) != 0) {
        logf(LOG_WARN, "error setting SO_LINGER: %s", strerror(errno));
    }       

}
    
int
AX25Socket::close()
{
    logf(LOG_DEBUG, "closing socket in state %s", statetoa(state_));

    if (fd_ == -1) {
        logf(LOG_DEBUG,"socket already closed.");
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
AX25Socket::shutdown(int how)
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
      
    logf(LOG_DEBUG, "shutdown(%s) state %s", howstr, statetoa(state_));
    
    if (state_ == INIT || state_ == FINI) {
        ASSERT(fd_ == -1);
        return 0;
    }
    
    if (::shutdown(fd_, how) != 0) {
        logf(LOG_ERR, "error in shutdown(%s) state %s: %s",
             howstr, statetoa(state_), strerror(errno));
    }
    
    if (state_ == ESTABLISHED) {
        if (how == SHUT_RD) { set_state(RDCLOSED); }
        if (how == SHUT_WR) { set_state(WRCLOSED); }
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
AX25Socket::send(const char* bp, size_t len, int flags)
{
    return IO::send(fd_, bp, len, flags, get_notifier(), logpath_);
}

int
AX25Socket::sendto(char* bp, size_t len, int flags,
                 const std::string& call, std::vector<std::string>& digi_route)
{
    struct full_sockaddr_ax25 sax25;
    memset(&sax25, 0, sizeof(sax25));
    sax25.fsa_ax25.sax25_family = AF_AX25;
    via_route_ = digi_route;
    
    // DML TODO: deal with source route info
    
    const char* route[2];
    route[0] = call.c_str();
    route[1] = 0;
    
    // format the source-route to the destination address
    if (ax25_aton_arglist((const char**)route, &sax25) == -1) {
        logf(LOG_ERR, "invalid destination callsign or digipeater");
        return -1;
    }

    return IO::sendto(fd_, bp, len, flags, (sockaddr*)&sax25, 
                      sizeof(sax25), get_notifier(), logpath_);
}

int
AX25Socket::sendmsg(const struct msghdr* msg, int flags)
{
    return IO::sendmsg(fd_, msg, flags, get_notifier(), logpath_);
}

int
AX25Socket::recv(char* bp, size_t len, int flags)
{
    return IO::recv(fd_, bp, len, flags, 
                    get_notifier(), logpath_);
}

int
AX25Socket::recvfrom(char* bp, size_t len, int flags,
                    std::string* addr)
{
    assert(NULL != addr);
    struct full_sockaddr_ax25 sax25;
    memset(&sax25, 0, sizeof(sax25));
    socklen_t sl = sizeof(sax25);
    
    int cc = IO::recvfrom(fd_, bp, len, flags, (sockaddr*)&sax25, &sl, 
                          get_notifier(), logpath_);
    
    if (cc < 0) {
        if (cc != IOINTR)
            logf(LOG_ERR, "error in recvfrom(): %s", strerror(errno));
        return cc;
    }
    
    struct full_sockaddr_ax25 remote;
    socklen_t slen = sizeof remote;
    if (::getpeername(fd_, (struct sockaddr *)&remote, &slen) == 0) 
       *addr = 
         ax25_ntoa((const ax25_address*)sax25.fsa_ax25.sax25_call.ax25_call);
    return cc;
}

int
AX25Socket::recvmsg(struct msghdr* msg, int flags)
{
    return IO::recvmsg(fd_, msg, flags, get_notifier(), logpath_);
}

int
AX25Socket::poll_sockfd(int events, int* revents, int timeout_ms)
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

#endif /* #ifdef OASYS_AX25_ENABLED  */
