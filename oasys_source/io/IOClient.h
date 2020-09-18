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


#ifndef _OASYS_IOCLIENT_H_
#define _OASYS_IOCLIENT_H_

#include <sys/types.h>
#include <sys/uio.h>

#include "IO.h"

namespace oasys {

/**
 * Abstract interface for any stream type output channel.
 */
class IOClient : virtual public IOHandlerBase {
public:
    virtual ~IOClient() {}

    //@{
    /**
     * System call wrappers.
     */
    virtual int read(char* bp, size_t len)                  = 0;
    virtual int write(const char* bp, size_t len)           = 0;
    virtual int readv(const struct iovec* iov, int iovcnt)  = 0;
    virtual int writev(const struct iovec* iov, int iovcnt) = 0;
    //@}
    
    //@{
    /**
     * Read/write out the entire supplied buffer, potentially
     * requiring multiple system calls.
     *
     * @return the total number of bytes read/written, or -1 on error
     */
    virtual int readall(char* bp, size_t len)                  = 0;
    virtual int writeall(const char* bp, size_t len)           = 0;
    virtual int readvall(const struct iovec* iov, int iovcnt)  = 0;
    virtual int writevall(const struct iovec* iov, int iovcnt) = 0;
    //@}

    //@{
    /**
     * @brief Try to read/write the specified number of bytes, but don't
     * block for more than timeout milliseconds.
     *
     * @return the number of bytes read/written or the appropriate
     * IOTimeoutReturn_t code
     */
    virtual int timeout_read(char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms) = 0;
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms) = 0;
    virtual int timeout_write(const char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_writev(const struct iovec* iov, int iovcnt,
                               int timeout_ms) = 0;
    virtual int timeout_writeall(const char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_writevall(const struct iovec* iov, int iovcnt,
                                  int timeout_ms) = 0;
    //@}

    //! Set the file descriptor's nonblocking status
    virtual int get_nonblocking(bool* nonblockingp) = 0;
    virtual int set_nonblocking(bool nonblocking)   = 0;
};

} // namespace oasys

#endif // _OASYS_IOCLIENT_H_
