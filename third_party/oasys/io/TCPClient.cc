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

#include <stdlib.h>
#include <sys/poll.h>

#include "TCPClient.h"
#include "NetUtils.h"
#include "debug/DebugUtils.h"
#include "debug/Log.h"

namespace oasys {

TCPClient::TCPClient(const char* logbase, bool init_socket_immediately)
    : IPClient(SOCK_STREAM, logbase)
{
    if (init_socket_immediately) {
        init_socket();
        ASSERT(fd_ != -1);
    }
}

TCPClient::TCPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
                     const char* logbase)
    : IPClient(SOCK_STREAM, fd, remote_addr, remote_port, logbase)
{
}

int
TCPClient::timeout_connect(in_addr_t remote_addr, u_int16_t remote_port,
                           int timeout_ms, int* errp)
{
    int ret, err;
    socklen_t len = sizeof(err);

    if (fd_ == -1) init_socket();

    if (IO::set_nonblocking(fd_, true, logpath_) < 0) {
        log_err("error setting fd %d to nonblocking: %s",
                fd_, strerror(errno));
        if (errp) *errp = errno;
        return IOERROR;
    }
    
    ret = IPSocket::connect(remote_addr, remote_port);
    
    if (ret == 0)
    {
        log_debug("timeout_connect: succeeded immediately");
        if (errp) *errp = 0;
        ASSERT(state_ == ESTABLISHED); // set by IPSocket
    }
    else if (ret < 0 && errno != EINPROGRESS)
    {
        log_err("timeout_connect: error from connect: %s",
                strerror(errno));
        if (errp) *errp = errno;
        ret = IOERROR;
    }
    else
    {
        ASSERT(errno == EINPROGRESS);
        log_debug("EINPROGRESS from connect(), calling poll()");
        ret = IO::poll_single(fd_, POLLOUT, NULL, timeout_ms, 
                              get_notifier(), logpath_);
        
        if (ret == IOTIMEOUT)
        {
            log_debug("timeout_connect: poll timeout");
        }
        else if (ret < 0) 
        {
            log_err("error in poll(): %s", strerror(errno));
            if (errp) *errp = errno;
            ret = IOERROR;
        }
        else
        {
            ASSERT(ret == 1);
            // call getsockopt() to see if connect succeeded
            ret = getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len);
            ASSERT(ret == 0);
            if (err == 0) {
                log_debug("return from poll, connect succeeded");
                ret = 0;
                set_state(ESTABLISHED);
            } else {
                log_debug("return from poll, connect failed");
                ret = IOERROR;
            }
        }
    }

    // always make sure to set the fd back to blocking
    if (IO::set_nonblocking(fd_, false, logpath_) < 0) {
        log_err("error setting fd %d back to blocking: %s",
                fd_, strerror(errno));
        if (errp) *errp = errno;
        return IOERROR;
    }
    
    monitor(IO::CONNECT, 0); // XXX/bowei

    return ret;
}

} // namespace oasys
