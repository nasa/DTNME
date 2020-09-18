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

#include "AX25Client.h"

namespace oasys {

/*****************************************************************************/
//                  Class AX25Client Implementation
/*****************************************************************************/

AX25Client::AX25Client(int socktype, const char* logbase, Notifier* intr)
    : IOHandlerBase(intr), AX25Socket(socktype, logbase)
{}

AX25Client::AX25Client(int socktype, int sock,
                        const std::string& remote_addr,
                        const char* logbase, Notifier* intr)
    : IOHandlerBase(intr), 
      AX25Socket(socktype, sock, remote_addr, logbase)
{}

AX25Client::~AX25Client() {}

int
AX25Client::read(char* bp, size_t len)
{
    // debugging hookto make sure that callers can handle short reads
    // #define TEST_SHORT_READ
#ifdef TEST_SHORT_READ
    if (len > 64) {
        int rnd = Random::rand(len);
        ::logf("/test/shortread", LOG_DEBUG, 
               "read(%d) -> read(%d)", len, rnd);
        len = rnd;
    }
#endif

    int cc = IO::read(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::readv(const struct iovec* iov, int iovcnt)
{
    int cc = IO::readv(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::write(const char* bp, size_t len)
{
    int cc = IO::write(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::writev(const struct iovec* iov, int iovcnt)
{
    int cc = IO::writev(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::readall(char* bp, size_t len)
{
    int cc = IO::readall(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::writeall(const char* bp, size_t len)
{
    int cc = IO::writeall(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::readvall(const struct iovec* iov, int iovcnt)
{
    int cc = IO::readvall(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::writevall(const struct iovec* iov, int iovcnt)
{
    int cc = IO::writevall(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_read(char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_read(fd_, bp, len, timeout_ms, 
                            get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_readv(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, 
                             get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_readall(fd_, bp, len, timeout_ms, 
                               get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_readvall(const struct iovec* iov, int iovcnt, 
                             int timeout_ms)
{
    int cc = IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms, 
                                get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_write(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_write(fd_, bp, len, timeout_ms, 
                               get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_writev(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_writev(fd_, iov, iovcnt, timeout_ms, 
                                get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_writeall(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_writeall(fd_, bp, len, timeout_ms, 
                                  get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::timeout_writevall(const struct iovec* iov, int iovcnt,
                              int timeout_ms)
{
    int cc = IO::timeout_writevall(fd_, iov, iovcnt, timeout_ms, 
                                   get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
AX25Client::get_nonblocking(bool *nonblockingp)
{
    int cc = IO::get_nonblocking(fd_, nonblockingp, logpath_);
    return cc;
}

int
AX25Client::set_nonblocking(bool nonblocking)
{
    int cc = IO::set_nonblocking(fd_, nonblocking, logpath_);
    return cc;
}


} // namespace oasys

#endif /* #ifdef OASYS_AX25_ENABLED  */
