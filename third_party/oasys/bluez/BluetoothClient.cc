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
extern int errno;

#include "BluetoothClient.h"

namespace oasys {

BluetoothClient::BluetoothClient(int socktype, BluetoothSocket::proto_t proto,
                                 const char* logbase, Notifier* intr)
    : IOHandlerBase(intr),
      BluetoothSocket(socktype, proto, logbase)
{
}

BluetoothClient::BluetoothClient(int socktype, BluetoothSocket::proto_t proto,
                                 int fd, bdaddr_t remote_addr,
                                 u_int8_t remote_channel,
                                 const char* logbase, Notifier* intr)
    : IOHandlerBase(intr),
      BluetoothSocket(socktype, proto, fd, remote_addr,
                      remote_channel, logbase)
{
}

BluetoothClient::~BluetoothClient()
{
}

int
BluetoothClient::read(char* bp, size_t len)
{
    return IO::read(fd_, bp, len, get_notifier(), logpath_);
}

int
BluetoothClient::readv(const struct iovec* iov, int iovcnt)
{
    return IO::readv(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int
BluetoothClient::write(const char* bp, size_t len)
{
    return IO::write(fd_, bp, len, get_notifier(), logpath_);
}

int
BluetoothClient::writev(const struct iovec* iov, int iovcnt)
{
    return IO::writev(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int
BluetoothClient::readall(char* bp, size_t len)
{
    return IO::readall(fd_, bp, len, get_notifier(), logpath_);
}

int
BluetoothClient::writeall(const char* bp, size_t len)
{
    return IO::writeall(fd_, bp, len, get_notifier(), logpath_);
}

int
BluetoothClient::readvall(const struct iovec* iov, int iovcnt)
{
    return IO::readvall(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int
BluetoothClient::writevall(const struct iovec* iov, int iovcnt)
{
    return IO::writevall(fd_, iov, iovcnt, get_notifier(), logpath_);
}

int
BluetoothClient::timeout_read(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_read(fd_, bp, len, timeout_ms,
                            get_notifier(), logpath_);
}

int
BluetoothClient::timeout_readv(const struct iovec* iov,
                               int iovcnt,
                               int timeout_ms)
{
    return IO::timeout_readv(fd_, iov, iovcnt, timeout_ms, get_notifier(),
                             logpath_);
}

int
BluetoothClient::timeout_readall(char* bp, size_t len, int timeout_ms)
{
    return IO::timeout_readall(fd_, bp, len, timeout_ms, get_notifier(),
                               logpath_);
}

int
BluetoothClient::timeout_readvall(const struct iovec* iov, int iovcnt,
                                  int timeout_ms)
{
    return IO::timeout_readvall(fd_, iov, iovcnt, timeout_ms, get_notifier(),
                                logpath_);
}

int
BluetoothClient::timeout_write(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_write(fd_, bp, len, timeout_ms,
                               get_notifier(), logpath_);
    return cc;
}

int
BluetoothClient::timeout_writev(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_writev(fd_, iov, iovcnt, timeout_ms,
                                get_notifier(), logpath_);
    return cc;
}

int
BluetoothClient::timeout_writeall(const char* bp, size_t len, int timeout_ms)
{
    int cc = IO::timeout_writeall(fd_, bp, len, timeout_ms,
                                  get_notifier(), logpath_);
    return cc;
}

int
BluetoothClient::timeout_writevall(const struct iovec* iov, int iovcnt, int timeout_ms)
{
    int cc = IO::timeout_writevall(fd_, iov, iovcnt, timeout_ms,
                                   get_notifier(), logpath_);
    return cc;
}

int
BluetoothClient::get_nonblocking(bool *nonblockingp)
{
    return IO::get_nonblocking(fd_, nonblockingp, logpath_);
}

int
BluetoothClient::set_nonblocking(bool nonblocking)
{
    ASSERT(fd_ != -1);
    return IO::set_nonblocking(fd_, nonblocking, logpath_);
}

} // namespace oasys
#endif /* OASYS_BLUETOOTH_ENABLED */
