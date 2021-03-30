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


#ifndef _OASYS_IO_H_
#define _OASYS_IO_H_

#include <fcntl.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "../debug/DebugUtils.h"
#include "../thread/Notifier.h"

namespace oasys {

class IOMonitor;

/**
 * Return code values for the timeout enabled functions such as
 * timeout_read() and timeout_accept(). Note that the functions return
 * an int, not an enumerated type since they may return other
 * information, e.g. the number of bytes read.
 */
enum IOTimeoutReturn_t {
    IOEOF       = 0,    ///< eof
    IOERROR     = -1,   ///< error
    IOTIMEOUT   = -2,   ///< timeout
    IOINTR      = -3,   ///< interrupted by notifier
    IOAGAIN     = -4,   ///< non-blocking sock got EAGAIN
    IORATELIMIT = -5,   ///< rate limited socket has no space
};


struct IO {
    //! Op code used by general IO functions
    enum IO_Op_t {
        READV = 1,
        RECV,
        RECVFROM,
        RECVMSG,
        WRITEV,
        SEND,
        SENDTO,
        SENDMSG,

        CONNECT,
        ACCEPT,
    };
    
    enum IO_Mmap_t {
        MMAP_RO,
        MMAP_RW,
    };

    /**
     * @return Text for the io error.
     */
    static const char* ioerr2str(int err);

    //@{
    /// System call wrappers (for logging)
    static int open(const char* path, int flags,
                    int* errnop = 0, const char* log = 0);
    
    static int open(const char* path, int flags, mode_t mode,
                    int* errnop = 0, const char* log = 0);
    
    static int close(int fd,
                     const char* log = 0,
                     const char* filename = "");
    
    static int unlink(const char* path, 
                      const char* log = 0);    

    static int lseek(int fd, off_t offset, int whence,
                     const char* log = 0);
    
    static int truncate(int fd, off_t length,
                        const char* log = 0);

    static int mkstemp(char* templ, const char* log = 0);

    static int stat(const char* path, struct stat* buf, const char* log = 0);

    static int lstat(const char* path, struct stat* buf, const char* log = 0);
    
    static void* mmap(int file, off_t offset, size_t length, IO_Mmap_t mode,
                      const char* log = 0);
    
    static int munmap(void* start, size_t length, const char* log = 0);
    
    static int mkdir(const char* path, mode_t mode, const char* log = 0);

    //@}

    //! @{ XXX/bowei - more documentation
    static int read(int fd, char* bp, size_t len,
                    Notifier* intr = 0, const char* log = 0);    

    static int readv(int fd, const struct iovec* iov, int iovcnt,
                     Notifier* intr = 0, const char* log = 0);

    static int readall(int fd, char* bp, size_t len,
                       Notifier* intr = 0, const char* log = 0);

    static int readvall(int fd, const struct iovec* iov, int iovcnt,
                        Notifier* intr = 0, const char* log = 0);

    static int timeout_read(int fd, char* bp, size_t len, int timeout_ms,
                            Notifier* intr = 0, const char* log  = 0);

    static int timeout_readv(int fd, const struct iovec* iov, int iovcnt,
                             int timeout_ms, Notifier* intr = 0, 
                             const char* log = 0);

    static int timeout_readall(int fd, char* bp, size_t len, int timeout_ms,
                               Notifier* intr = 0, const char* log = 0);

    static int timeout_readvall(int fd, const struct iovec* iov, int iovcnt,
                                int timeout_ms, Notifier* intr = 0, 
                                const char* log = 0);

    static int recv(int fd, char* bp, size_t len, int flags,
                    Notifier* intr = 0,  const char* log = 0);

    static int recvfrom(int fd, char* bp, size_t len,
                        int flags, struct sockaddr* from, socklen_t* fromlen,
                        Notifier* intr = 0, const char* log = 0);

    static int recvmsg(int fd, struct msghdr* msg, int flags,
                       Notifier* intr = 0, const char* log = 0);
    
    static int write(int fd, const char* bp, size_t len,
                     Notifier* intr = 0, const char* log = 0);

    static int writev(int fd, const struct iovec* iov, int iovcnt,
                      Notifier* intr = 0, const char* log = 0);

    static int writeall(int fd, const char* bp, size_t len,
                        Notifier* intr = 0, const char* log = 0);

    static int writevall(int fd, const struct iovec* iov, int iovcnt,
                         Notifier* intr = 0, const char* log = 0);

    static int timeout_write(int fd, const char* bp, size_t len, int timeout_ms,
                             Notifier* intr = 0, const char* log = 0);

    static int timeout_writev(int fd, const struct iovec* iov, int iovcnt, 
                              int timeout_ms, Notifier* intr = 0, 
                              const char* log = 0);

    static int timeout_writeall(int fd, const char* bp, size_t len, 
                                int timeout_ms,
                                Notifier* intr = 0, const char* log = 0);

    static int timeout_writevall(int fd, const struct iovec* iov, int iovcnt,
                                 int timeout_ms, Notifier* intr = 0, 
                                 const char* log = 0);
    
    static int send(int fd, const char* bp, size_t len, int flags,
                    Notifier* intr = 0, const char* log = 0);
    
    static int sendto(int fd, char* bp, size_t len, 
                      int flags, const struct sockaddr* to, socklen_t tolen,
                      Notifier* intr = 0, const char* log = 0);

    static int sendmsg(int fd, const struct msghdr* msg, int flags,
                       Notifier* intr = 0, const char* log = 0);
    //! @}

    //! @return IOTIMEOUT, IOINTR, 1 indicates readiness, otherwise
    //! it's an error.
    static int poll_single(int fd, short events, short* revents, 
                           int timeout_ms,
                           Notifier* intr = 0, const char* log = 0);

    //! @return IOTIMEOUT, IOINTR, 1 indicates readiness, otherwise
    //! it's an error
    static int poll_multiple(struct pollfd* fds, int nfds, int timeout_ms,
                             Notifier* intr = 0, const char* log = 0);

    //! @{ Read/Write in the entire supplied buffer, potentially !
    //! requiring multiple system calls
    //! @}

    //! @{ Get and Set the file descriptor's nonblocking status
    static int get_nonblocking(int fd, bool *nonblocking,
                               const char* log = NULL);
    static int set_nonblocking(int fd, bool nonblocking,
                               const char* log = NULL);
    //! @}

    /**
     * @return total bytes in the iovec to be written
     */
    static size_t iovec_size(const struct iovec* iov, int num) {
        size_t size = 0;
        for (int i=0; i<num; ++i) {
            size += iov[i].iov_len;
        }
        return size;
    }
    
    
    //! Poll on an fd, interruptable by the notifier.
    static int poll_with_notifier(Notifier*             intr, 
                                  struct pollfd*        fds,
                                  size_t                nfds,
                                  int                   timeout,  
                                  const struct timeval* start_time,
                                  const char*           log);

    //! Union used to pass extra arguments to rwdata
    union RwDataExtraArgs {
        const struct msghdr* sendmsg_hdr;

        struct msghdr* recvmsg_hdr;
        
        struct {
            const struct sockaddr* to;
            socklen_t tolen;
        } sendto;
    
        struct {
            struct sockaddr* from;
            socklen_t* fromlen;
        } recvfrom;
    };

    //! This is the do all function which will (depending on the flags
    //! given dispatch to the correct read/write/send/rcv call
    static int rwdata(IO_Op_t               op, 
                      int                   fd, 
                      const struct iovec*   iov, 
                      int                   iovcnt, 
                      int                   flags, 
                      int                   timeout,
                      RwDataExtraArgs*      args,
                      const struct timeval* start_time,
                      Notifier*             intr, 
                      bool                  ignore_eagain,
                      const char*           log);
    
    //! Do all function for iovec reading/writing
    static int rwvall(IO_Op_t               op, 
                      int                   fd, 
                      const struct iovec*   iov, 
                      int                   iovcnt,
                      int                   timeout,
                      const struct timeval* start,
                      Notifier*             intr, 
                      const char*           fcn_name, 
                      const char*           log);
    
    //! Adjust the timeout value given a particular start time
    static int adjust_timeout(int timeout, const struct timeval* start);
}; // class IO

//! Class used to intercept I/O operations for monitoring purposes
class IOMonitor {
public:
    struct info_t {
        int timeout_ms_;
        int err_code_;
        // XXX/bowei - todo
    };

    virtual ~IOMonitor() {}
    virtual void monitor(IO::IO_Op_t op, const info_t* info) = 0;
};

//! Virtually inherited base class for holding common elements of an
//! I/O handling class. IOHandlerBase will possess the notifier
//! associated with it XXX/bowei - don't know whether this is
//! appropriate
class IOHandlerBase {
public:
    IOHandlerBase(Notifier* intr = 0) 
        : intr_(intr), monitor_(0) {}
    ~IOHandlerBase() { delete_z(intr_); }

    Notifier* get_notifier() { 
        return intr_; 
    }

    void interrupt_from_io() {
        ASSERT(intr_ != 0);
        intr_->notify();
    }
    
    void set_notifier(Notifier* intr) { 
        ASSERT(intr_ == 0);
        intr_ = intr; 
    }

    void set_monitor(IOMonitor* monitor) {
        monitor_ = monitor;
    }
    
    void monitor(IO::IO_Op_t op, 
                 const IOMonitor::info_t* info) 
    {
        if (monitor_ != 0) {
            monitor_->monitor(op, info);
        }
    }

private:
    Notifier* intr_;
    IOMonitor*  monitor_;
};

} // namespace oasys

#endif /* _OASYS_IO_H_ */

