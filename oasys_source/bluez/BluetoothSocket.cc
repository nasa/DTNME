/*
 *    Copyright 2006 Baylor University
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

#ifdef OASYS_BLUETOOTH_ENABLED

#include <cerrno>
#include "BluetoothSocket.h"

namespace oasys {

int BluetoothSocket::abort_on_error_ = 0;

BluetoothSocket::BluetoothSocket(int socktype,
                                 proto_t proto,
                                 const char* logbase)
    : Logger("BluetoothSocket", logbase)
{
    state_ = INIT;
    memset(&local_addr_,0,sizeof(bdaddr_t));
    channel_ = 0;
    memset(&remote_addr_,0,sizeof(bdaddr_t));
    fd_ = -1;
    socktype_ = socktype;
    proto_ = (proto_t) proto;
    logfd_ = true; 
}

BluetoothSocket::BluetoothSocket(int socktype, proto_t proto, int sock,
                   bdaddr_t remote_addr, u_int8_t channel,
                   const char* logbase)
    : Logger("BluetoothSocket", logbase)
{
    fd_ = sock;
    proto_ = (proto_t) proto;
    logpathf("%s/%s/%d",logbase,prototoa((proto_t)proto_),sock);
    socktype_ = socktype;
    state_ = ESTABLISHED;
    bacpy(&local_addr_,BDADDR_ANY);
    set_channel(channel);
    set_remote_addr(remote_addr);

    configure();
}

BluetoothSocket::~BluetoothSocket()
{
    close();
}

void
BluetoothSocket::init_socket()
{
    // should only be called at real init time or after a call to close()
    ASSERT(state_ == INIT || state_ == FINI);
    ASSERT(fd_ == -1);
    state_ = INIT;

    fd_ = socket(PF_BLUETOOTH, socktype_, (int) proto_);
    if (fd_ == -1) {
        logf(LOG_ERR, "error creating socket: %s", strerror(errno));
        if(errno==EBADFD) close();
        return;
    }

    if (logfd_)
        Logger::logpath_appendf("/%s/%d",
                                prototoa((proto_t)proto_),
                                fd_);

    logf(LOG_DEBUG, "created socket %d of protocol %s", fd_, 
         prototoa((proto_t)proto_));

    configure(); 
}

const char*
BluetoothSocket::statetoa(state_t state)
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
BluetoothSocket::set_state(state_t state)
{
    logf(LOG_DEBUG, "state %s -> %s", statetoa(state_), 
         statetoa(state));
    state_ = state;
}

const char* 
BluetoothSocket::prototoa(proto_t proto)
{
    switch (proto) {
        case L2CAP:    return "L2CAP";
        case HCI:      return "HCI";
        case SCO:      return "SCO";
        case RFCOMM:   return "RFCOMM";
        case BNEP:     return "BNEP";
        case CMTP:     return "CMTP";
        case HIDP:     return "HIDP";
        case AVDTP:    return "AVDTP";
    }
    ASSERT(0);
    return NULL;
}

void 
BluetoothSocket::set_proto(proto_t proto)
{
    logf(LOG_DEBUG, "protocol %s -> %s", prototoa((proto_t)proto_), 
         prototoa((proto_t)proto));
    proto_ = proto;
}

int
BluetoothSocket::bind(bdaddr_t local_addr, u_int8_t local_channel)
{
    struct sockaddr sa;

    if (fd_ == -1) init_socket();

    set_local_addr(local_addr);
    set_channel(local_channel);

    if (!params_.silent_connect_)
    logf(LOG_DEBUG, "binding to %s(%d)", bd2str(local_addr),local_channel);

    memset(&sa,0,sizeof(sa));
    switch(proto_) {
    case RFCOMM:
        // from net/bluetooth/rfcomm/core.c
        ASSERT(channel_ >= 1 && channel_ <= 30);
        rc_ = (struct sockaddr_rc*)&sa;
        rc_->rc_family = AF_BLUETOOTH;
        rc_->rc_channel = channel_;
        bacpy(&rc_->rc_bdaddr,&local_addr_);
        break;
    default:
        ASSERTF(0,"unsupported protocol %s",prototoa((proto_t)proto_));
        break;
    }

    if(::bind(fd_,&sa,sizeof(sa)) != 0) {
        log_level_t level = LOG_ERR;
        if(errno == EADDRINUSE) level = LOG_DEBUG;
        if (!params_.silent_connect_)
        logf(level, "failed to bind to %s(%d): %s",
             bd2str(local_addr), channel_, strerror(errno));
        if(errno==EBADFD) close();
        return -1;
    }

    return 0;
}

int
BluetoothSocket::bind()
{
    return bind(local_addr_,channel_);
}

int
BluetoothSocket::connect()
{
    // In Bluetooth, piconets are formed by one Master
    // connecting to up to seven Slaves, simultaneously
    // The device performing connect() is Master

    struct sockaddr sa;

    if (state_ == ESTABLISHED) 
        return 0;

    if (fd_ == -1) init_socket();

    log_debug("connecting to %s(%d)",bd2str(remote_addr_),channel_);

    memset(&sa,0,sizeof(sa));
    switch(proto_) {
    case RFCOMM:
        // from net/bluetooth/rfcomm/core.c
        ASSERT(channel_ >= 1 && channel_ <= 30);
        rc_ = (struct sockaddr_rc*)&sa;
        rc_->rc_family = AF_BLUETOOTH;
        rc_->rc_channel = channel_;
        bacpy(&rc_->rc_bdaddr,&remote_addr_);
        break;
    default:
        ASSERTF(0,"unsupported protocol %s",prototoa((proto_t)proto_));
        break;
    }

    set_state(CONNECTING);

    if (::connect(fd_,&sa,sizeof(sa)) < 0) {
        if (errno == EISCONN && !params_.silent_connect_)
            log_debug("already connected to %s-%u",
                      bd2str(remote_addr_), channel_);
        else if (errno == EINPROGRESS && !params_.silent_connect_) {
            log_debug("delayed connect to %s-%u",
                      bd2str(remote_addr_), channel_);
        } else if(errno==EBADFD) {
            if (!params_.silent_connect_) log_err("EBADFD");
            close();
        } else {
            if (!params_.silent_connect_)
                log_debug("error connecting to %s(%d): %s",
                          bd2str(remote_addr_), channel_, strerror(errno));
        }
        return -1;
    }

    set_state(ESTABLISHED);

    return 0;
}

int
BluetoothSocket::async_connect_result()
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
        state_ = ESTABLISHED;
    }

    return result;
}

int
BluetoothSocket::connect(bdaddr_t remote_addr, u_int8_t remote_channel)
{
    set_remote_addr(remote_addr);
    set_channel(remote_channel);

    return connect();
}

void
BluetoothSocket::configure()
{
    ASSERT(fd_ != -1);

    if (params_.reuseaddr_) {
        int y = 1;
        logf(LOG_DEBUG, "setting SO_REUSEADDR");
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &y, sizeof y) != 0) {
            logf(LOG_WARN, "error setting SO_REUSEADDR: %s",
                 strerror(errno));
        }
    }

    if (params_.recv_bufsize_ > 0) {
        logf(LOG_DEBUG, "setting SO_RCVBUF to %d",
             params_.recv_bufsize_);
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF,
                         &params_.recv_bufsize_,
                         sizeof(params_.recv_bufsize_)) < 0)
        {
            logf(LOG_WARN, "error setting SO_RCVBUF to %d: %s",
                 params_.recv_bufsize_, strerror(errno)); 
        }
    }

    if (params_.send_bufsize_ > 0) {
        logf(LOG_DEBUG, "setting SO_SNDBUF to %d",
             params_.send_bufsize_);
        if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF,
                         &params_.send_bufsize_,
                         sizeof(params_.send_bufsize_)) < 0)
        {
            logf(LOG_WARN, "error setting SO_SNDBUF to %d: %s",
                 params_.send_bufsize_, strerror(errno)); 
        }
    }

}

int
BluetoothSocket::close()
{
    logf(LOG_DEBUG, "closing socket in state %s", statetoa(state_));

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
BluetoothSocket::shutdown(int how)
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
BluetoothSocket::send(const char* bp, size_t len, int flags)
{
    return IO::send(fd_, bp, len, flags, get_notifier(), logpath_);
}

int
BluetoothSocket::recv(char* bp, size_t len, int flags)
{
    return IO::recv(fd_, bp, len, flags, get_notifier(), logpath_);
}

int
BluetoothSocket::fd()
{
    return fd_;
}

void
BluetoothSocket::local_addr(bdaddr_t& addr)
{
    if (!bacmp(&addr,&local_addr_)) get_local();
    bacpy(&addr,&local_addr_);
}

u_int8_t
BluetoothSocket::channel()
{
    if (channel_ == 0) get_local();
    return channel_;
}

void
BluetoothSocket::remote_addr(bdaddr_t& addr)
{
    if (!bacmp(&addr,&remote_addr_)) get_remote();
    bacpy(&addr,&remote_addr_);
}

void
BluetoothSocket::set_local_addr(bdaddr_t& addr)
{
    bacpy(&local_addr_,&addr);
}

void
BluetoothSocket::set_channel(u_int8_t channel)
{
    // from net/bluetooth/rfcomm/core.c
    ASSERT(channel >= 1 && channel <= 30);
    channel_ = channel;
}

void
BluetoothSocket::set_remote_addr(bdaddr_t& addr)
{
    bacpy(&remote_addr_,&addr);
}

void
BluetoothSocket::get_local()
{
    if (fd_ < 0)
        return;

    socklen_t slen = sizeof(struct sockaddr);
    struct sockaddr sa;
    memset(&sa,0,slen);
    if(::getsockname(fd_, &sa, &slen) == 0) {
        switch (proto_) { 
            case RFCOMM:
                rc_ = (struct sockaddr_rc *) &sa;
                bacpy(&local_addr_,&rc_->rc_bdaddr);
                channel_ = rc_->rc_channel;
                break;
            // not implemented
            default:
                ASSERTF(0,"not implemented for %s",prototoa((proto_t)proto_));
                break;
        }
    }
}

void
BluetoothSocket::get_remote()
{
    if (fd_ < 0)
        return;

    socklen_t slen = sizeof(struct sockaddr);
    struct sockaddr sa;
    memset(&sa,0,slen);
    if(::getpeername(fd_, &sa, &slen) == 0) {
        switch (proto_) {
            case RFCOMM:
                rc_ = (struct sockaddr_rc *) &sa;
                bacpy(&remote_addr_,&rc_->rc_bdaddr);
                channel_ = rc_->rc_channel;
                break;
            // not implemented
            default:
                ASSERTF(0,"not implemented for %s",prototoa((proto_t)proto_));
                break;
        }
    }
}

int
BluetoothSocket::poll_sockfd(int events, int* revents, int timeout_ms)
{
    short s_events = events;
    short s_revents = 0;

    int cc = IO::poll_single(fd_, s_events, &s_revents, timeout_ms,
                             get_notifier(), logpath_);

    if (revents != 0) {
        *revents = s_revents;
    }

    return cc;
}

} // namespace oasys
#endif /* OASYS_BLUETOOTH_ENABLED */
