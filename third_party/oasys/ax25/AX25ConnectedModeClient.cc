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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

#include "io/NetUtils.h"
#include "util/Random.h"
#include "debug/Log.h"
#include "debug/DebugUtils.h"

#include "AX25ConnectedModeClient.h"

namespace oasys {

/*****************************************************************************/
//                  Class AX25ConnectedModeClient Implementation
/*****************************************************************************/
AX25ConnectedModeClient::AX25ConnectedModeClient(const char* logbase,
                                                 bool init_socket_immediately)
    : AX25Client(SOCK_SEQPACKET, logbase)
{
    if (init_socket_immediately) {
        init_socket();
        ASSERT(fd_ != -1);
    }
}

AX25ConnectedModeClient::AX25ConnectedModeClient(int fd, const std::string& remote_addr, 
                                                 const char* logbase)
    : AX25Client(SOCK_STREAM, fd, remote_addr, logbase)
{}

int
AX25ConnectedModeClient::timeout_connect(const std::string& axport, 
                                        const std::string& remote_addr, 
                                        std::vector<std::string> digi_list,
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
    
    ret = AX25Socket::connect(axport,remote_addr, digi_list);
    
    if (ret == 0)
    {
        log_debug("timeout_connect: succeeded immediately");
        if (errp) *errp = 0;
        ASSERT(state_ == ESTABLISHED); // set by AX25Socket
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

#endif /* #ifdef OASYS_AX25_ENABLED  */
