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

#include "IPClient.h"
#include "util/Random.h"

namespace oasys {

IPClient::IPClient(int socktype, const char* logbase, Notifier* intr)
    : IOHandlerBase(intr), IPSocket(socktype, logbase)
{}

IPClient::IPClient(int socktype, int sock,
                   in_addr_t remote_addr, u_int16_t remote_port,
                   const char* logbase, Notifier* intr)
    : IOHandlerBase(intr), 
      IPSocket(socktype, sock, remote_addr, remote_port, logbase)
{}

IPClient::~IPClient() {}

int
IPClient::read(char* bp, size_t len)
{
    // debugging hookto make sure that callers can handle short reads
    // #define TEST_SHORT_READ
#ifdef TEST_SHORT_READ
    if (len > 64) {
        int rnd = Random::rand(len);
        ::logf("/test/shortread", LOG_DEBUG, "read(%d) -> read(%d)", len, rnd);
        len = rnd;
    }
#endif

    int cc = IO::read(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::readv(const struct iovec* iov, int iovcnt)
{
    int cc = IO::readv(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::write(const char* bp, size_t len)
{
    int cc = IO::write(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::writev(const struct iovec* iov, int iovcnt)
{
    int cc = IO::writev(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::readall(char* bp, size_t len)
{
    int cc = IO::readall(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::writeall(const char* bp, size_t len)
{
    int cc = IO::writeall(fd_, bp, len, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::readvall(const struct iovec* iov, int iovcnt)
{
    int cc = IO::readvall(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::writevall(const struct iovec* iov, int iovcnt)
{
    int cc = IO::writevall(fd_, iov, iovcnt, get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_read(fd_, bp, len, timeout_ms, 
                            get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_readv(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, 
                             get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_readall(fd_, bp, len, timeout_ms, 
                               get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_readvall(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms, 
                                get_notifier(), logpath_);
    monitor(IO::READV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_write(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_write(fd_, bp, len, timeout_ms, 
                               get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_writev(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_writev(fd_, iov, iovcnt, timeout_ms, 
                                get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_writeall(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_writeall(fd_, bp, len, timeout_ms, 
                                  get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::timeout_writevall(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_writevall(fd_, iov, iovcnt, timeout_ms, 
                                   get_notifier(), logpath_);
    monitor(IO::WRITEV, 0); // XXX/bowei

    return cc;
}

int
IPClient::get_nonblocking(bool *nonblockingp)
{
    int cc = IO::get_nonblocking(fd_, nonblockingp, logpath_);
    return cc;
}

int
IPClient::set_nonblocking(bool nonblocking)
{
    int cc = IO::set_nonblocking(fd_, nonblocking, logpath_);
    return cc;
}

} // namespace oasys
