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

#include "FdIOClient.h"
#include "IO.h"
#include "debug/DebugUtils.h"

namespace oasys {

FdIOClient::FdIOClient(int fd, Notifier* intr, const char* logpath)
    : IOHandlerBase(intr), 
      Logger("FdIOClient", "%s", logpath), 
      fd_(fd)
{}

int 
FdIOClient::read(char* bp, size_t len)
{
    return IO::read(fd_, bp, len, get_notifier(), logpath_);
}

int 
FdIOClient::readv(const struct iovec* iov, int iovcnt)
{
    return IO::readv(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int 
FdIOClient::readall(char* bp, size_t len)
{
    return IO::readall(fd_, bp, len, get_notifier(), logpath_);
}

int 
FdIOClient::readvall(const struct iovec* iov, int iovcnt)
{
    return IO::readvall(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int 
FdIOClient::write(const char* bp, size_t len)
{
    return IO::write(fd_, bp, len, get_notifier(), logpath_);
}

int
FdIOClient::writev(const struct iovec* iov, int iovcnt)
{
    return IO::writev(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int 
FdIOClient::writeall(const char* bp, size_t len)
{
    return IO::writeall(fd_, bp, len, get_notifier(), logpath_);
}

int 
FdIOClient::writevall(const struct iovec* iov, int iovcnt)
{
    return IO::writevall(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int 
FdIOClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_read(fd_, bp, len, timeout_ms,
                            get_notifier(), logpath_);
}

int 
FdIOClient::timeout_readv(const struct iovec* iov, int iovcnt,
                  int timeout_ms)
{
    return IO::timeout_readv(fd_, iov, iovcnt, timeout_ms,
                             get_notifier(), logpath_);
}

int
FdIOClient::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_readall(fd_, bp, len, timeout_ms,
                               get_notifier(), logpath_);
}

int
FdIOClient::timeout_readvall(const struct iovec* iov, int iovcnt,
                             int timeout_ms)
{
    return IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms,
                                get_notifier(), logpath_);
}

int 
FdIOClient::timeout_write(const char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_write(fd_, bp, len, timeout_ms,
                             get_notifier(), logpath_);
}

int 
FdIOClient::timeout_writev(const struct iovec* iov, int iovcnt,
                           int timeout_ms)
{
    return IO::timeout_writev(fd_, iov, iovcnt, timeout_ms,
                              get_notifier(), logpath_);
}

int
FdIOClient::timeout_writeall(const char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_writeall(fd_, bp, len, timeout_ms,
                                get_notifier(), logpath_);
}

int
FdIOClient::timeout_writevall(const struct iovec* iov, int iovcnt,
                             int timeout_ms)
{
    return IO::timeout_writevall(fd_, iov, iovcnt, timeout_ms,
                                 get_notifier(), logpath_);
}

int
FdIOClient::get_nonblocking(bool* nonblockingp)
{
    return IO::get_nonblocking(fd_, nonblockingp, logpath_);
}

int
FdIOClient::set_nonblocking(bool nonblocking)
{
    return IO::set_nonblocking(fd_, nonblocking, logpath_);
}

void
FdIOClient::sync()
{
    fsync(fd_);
}

} // namespace oasys
