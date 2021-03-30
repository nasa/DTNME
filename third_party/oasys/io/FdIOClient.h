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


#ifndef _OASYS_FD_IOCLIENT_H_
#define _OASYS_FD_IOCLIENT_H_

#include "IOClient.h"
#include "../debug/Logger.h"
#include "../thread/Notifier.h"

namespace oasys {

/**
 * IOClient which uses pure file descriptors.
 */
class FdIOClient : public IOClient, public Logger {
public:
    //! @param fd File descriptor to interact with
    //! @param intr Optional notifier to use to interrupt blocked I/O
    FdIOClient(int fd, Notifier* intr = 0,
               const char* logpath   = "/oasys/io/FdIOClient");

    //! Explicitly set the file descriptor
    void set_fd(int fd) { fd_ = fd; }

    //! Accessor for the file descriptor
    int fd() const { return fd_; }

    //! @{ Virtual from IOClient
    virtual int read(char* bp, size_t len);
    virtual int readv(const struct iovec* iov, int iovcnt);
    virtual int write(const char* bp, size_t len);
    virtual int writev(const struct iovec* iov, int iovcnt);

    virtual int readall(char* bp, size_t len);
    virtual int writeall(const char* bp, size_t len);
    virtual int readvall(const struct iovec* iov, int iovcnt);
    virtual int writevall(const struct iovec* iov, int iovcnt);
    
    virtual int timeout_read(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms);
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms);

    virtual int timeout_write(const char* bp, size_t len, int timeout_ms);
    virtual int timeout_writev(const struct iovec* iov, int iovcnt,
                               int timeout_ms);
    virtual int timeout_writeall(const char* bp, size_t len, int timeout_ms);
    virtual int timeout_writevall(const struct iovec* iov, int iovcnt,
                                  int timeout_ms);

    virtual int get_nonblocking(bool* nonblockingp);
    virtual int set_nonblocking(bool nonblocking);
    //! @}

    /*!
     * Call to fsync.
     */
    void sync();

protected:
    int       fd_;
};

} // namespace oasys

#endif /* _OASYS_FD_IOCLIENT_H_ */
