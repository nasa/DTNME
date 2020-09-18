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

#include <errno.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "IO.h"

#include "debug/Log.h"
#include "util/ScratchBuffer.h"
#include "util/StringBuffer.h"
#include "thread/Notifier.h"

namespace oasys {

#if defined(_SC_PAGESIZE)
static int page_size = sysconf(_SC_PAGESIZE);
#else
static int page_size = ::getpagesize();
#endif

//----------------------------------------------------------------------------
//! Small helper class which is a copy-on-write iovec and also handle
//! adjustment for consumption of the bytes in the iovec.
class COWIoVec {
public:
    COWIoVec(const struct iovec* iov, int iovcnt) 
        : iov_(const_cast<struct iovec*>(iov)),
          iovcnt_(iovcnt),
          bytes_left_(0),
          copied_(false),
          dynamic_iov_(0)
    {
        for (int i=0; i<iovcnt_; ++i) {
            bytes_left_ += iov_[i].iov_len;
        }
    }

    ~COWIoVec() { 
        if (dynamic_iov_ != 0) {
            free(iov_); 
            dynamic_iov_ = 0;
        } 
    }
    
    /**
     * @return number of bytes left in the iovecs
     */
    void consume(size_t cc) {
        ASSERT(bytes_left_ >= cc);

        // common case, all the bytes are gone on the first run
        if (!copied_ && cc == bytes_left_) {
            iov_        = 0;
            bytes_left_ = 0;
            return;
        }
        
        if (!copied_) {
            copy();
        }
        
        // consume the iovecs
        bytes_left_ -= cc;
        while (cc > 0) {
            ASSERT(iovcnt_ > 0);

            if (iov_[0].iov_len <= cc) {
                cc -= iov_[0].iov_len;
                --iovcnt_;
                ++iov_;
            } else {
                iov_[0].iov_base = reinterpret_cast<char*>
                                   (iov_[0].iov_base) + cc;
                iov_[0].iov_len  -= cc;
                cc = 0;
                break;
            }
        }
        
        // For safety
        if (bytes_left_ == 0) {
            iov_ = 0;
        }
    }

    void copy() {
        ASSERT(!copied_);
        
        copied_ = true;
        if (iovcnt_ <= 16) {
            memcpy(static_iov_, iov_, 
                   iovcnt_ * sizeof(struct iovec));
            iov_ = static_iov_;
        } else {
            dynamic_iov_ = static_cast<struct iovec*>
                           (malloc(iovcnt_ * sizeof(struct iovec)));
            memcpy(dynamic_iov_, iov_, iovcnt_* sizeof(struct iovec));
            iov_ = dynamic_iov_;
        }
    }
    
    const struct iovec* iov()        { return iov_; }
    int                 iovcnt()     { return iovcnt_; }
    size_t              bytes_left() { return bytes_left_; }

private:
    struct iovec* iov_;
    int           iovcnt_;
    size_t        bytes_left_;
    
    bool          copied_;
    struct iovec  static_iov_[16];
    struct iovec* dynamic_iov_;
};

//----------------------------------------------------------------------------
const char* 
IO::ioerr2str(int err)
{
    switch (err) {
    case IOEOF:     return "eof";
    case IOERROR:   return "error";
    case IOTIMEOUT: return "timeout";
    case IOINTR:    return "intr";
    }
    
    NOTREACHED;
}

//----------------------------------------------------------------------------
int
IO::open(const char* path, int flags, int* errnop, const char* log)
{
    int fd = ::open(path, flags);
    if (errnop) *errnop = errno;
    
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "open %s (flags 0x%x): fd %d", path, flags, fd);
#endif
    }
    return fd;
}

//----------------------------------------------------------------------------
int
IO::open(const char* path, int flags, mode_t mode,
         int* errnop, const char* log)
{
    int fd = ::open(path, flags, mode);
    if (errnop) *errnop = errno;
    
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "open %s (flags 0x%x mode 0x%x): fd %d",
             path, flags, (u_int)mode, fd);
#endif
    }
    return fd;
}
    
//----------------------------------------------------------------------------
int
IO::close(int fd, const char* log, const char* filename)
{
    (void) filename;

    int ret = ::close(fd);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "close %s fd %d: %d", filename, fd, ret);
#endif
    }
    return ret;
}

//----------------------------------------------------------------------------
int
IO::unlink(const char* path, const char* log)
{
    int ret = ::unlink(path);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "unlink %s: %d", path, ret);
#endif
    }

    return ret;
}

//----------------------------------------------------------------------------
int
IO::lseek(int fd, off_t offset, int whence, const char* log)
{
    int cc = ::lseek(fd, offset, whence);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "lseek %lu %s -> %d",
             (long unsigned int)offset,
             (whence == SEEK_SET) ? "SEEK_SET" :
             (whence == SEEK_CUR) ? "SEEK_CUR" :
             (whence == SEEK_END) ? "SEEK_END" :
             "SEEK_INVALID",
             cc);
#endif
    }

    return cc;
}

//----------------------------------------------------------------------------
int
IO::truncate(int fd, off_t length, const char* log)
{
    int ret = ftruncate(fd, length);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "truncate %lu: %d", (long unsigned int)length, ret);
#endif
    }

    return ret;
}

//----------------------------------------------------------------------------
int
IO::mkstemp(char* templ, const char* log)
{
    int ret = ::mkstemp(templ);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "mkstemp %s: %d", templ, ret);
#endif
    }

    return ret;
}

//----------------------------------------------------------------------
int
IO::stat(const char* path, struct stat* buf, const char* log)
{
    int ret = ::stat(path, buf);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "stat %s: %d", path, ret);
#endif
    }
    
    return ret;
}

//----------------------------------------------------------------------
int
IO::lstat(const char* path, struct stat* buf, const char* log)
{
    int ret = ::lstat(path, buf);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "stat %s: %d", path, ret);
#endif
    }
    
    return ret;
}

void*
IO::mmap(int fd, off_t offset, size_t length, IO_Mmap_t mode, const char* log) 
{
    int prot = 0;
    int flags = 0;
    //int page_size = ::getpagesize();
    off_t map_offset = offset & ~(page_size - 1);
    
    switch (mode) {
    case MMAP_RO:
        prot = PROT_READ;
        flags = MAP_PRIVATE;
        break;
        
    case MMAP_RW:
        prot = PROT_READ | PROT_WRITE;
        flags = MAP_SHARED;
        break;
    }
            
    void* ptr = ::mmap(NULL, length, prot, flags, fd, map_offset);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "mmap: %p", ptr);
#endif
    }
    
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    
    return (u_int8_t*)ptr + (offset & (page_size - 1));  
}

int
IO::munmap(void* start, size_t length, const char* log)
{
    void* map_start = (void*)((unsigned long)start & ~(page_size - 1));
    size_t map_length = length + ((unsigned long)start & (page_size - 1));
    
    int ret = ::munmap(map_start, map_length);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "munmap %p, length %zu: %d", start, length, ret);
#endif
    }
    
    return ret;
}

int
IO::mkdir(const char* path, mode_t mode, const char* log)
{
    int ret = ::mkdir(path, mode);
    if (log) {
#ifdef OASYS_LOG_DEBUG_ENABLED
        logf(log, LOG_DEBUG, "mkdir %s: %d", path, ret);
#endif
    }
    
    if (ret < 0 && errno == EEXIST)
        return 0;
    
    return ret;
}

//----------------------------------------------------------------------------
int
IO::read(int fd, char* bp, size_t len, 
         Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;
    return rwdata(READV, fd, &iov, 1, 0, -1, 0, 0, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::readv(int fd, const struct iovec* iov, int iovcnt, 
          Notifier* intr, const char* log)
{
    return rwdata(READV, fd, iov, iovcnt, 0, -1, 0, 0, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::readall(int fd, char* bp, size_t len, 
            Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;

    return rwvall(READV, fd, &iov, 1, -1, 0, intr, "readall", log);
}

//----------------------------------------------------------------------------
int
IO::readvall(int fd, const struct iovec* iov, int iovcnt,
             Notifier* intr, const char* log)
{
    return rwvall(READV, fd, iov, iovcnt, -1, 0, intr, "readvall", log);
}


//----------------------------------------------------------------------------
int
IO::timeout_read(int fd, char* bp, size_t len, int timeout_ms,
                 Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;

    struct timeval start;
    gettimeofday(&start, 0);

    return rwdata(READV, fd, &iov, 1, 0, timeout_ms, 0, 
                  &start, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::timeout_readv(int fd, const struct iovec* iov, int iovcnt, int timeout_ms,
                  Notifier* intr, const char* log)
{
    struct timeval start;
    gettimeofday(&start, 0);

    return rwdata(READV, fd, iov, iovcnt, 0, timeout_ms, 0, 
                  &start, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::timeout_readall(int fd, char* bp, size_t len, int timeout_ms,
                    Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;

    struct timeval start;
    gettimeofday(&start, 0);    

    return rwvall(READV, fd, &iov, 1, timeout_ms, &start, 
                  intr, "timeout_readall", log);
}

//----------------------------------------------------------------------------
int
IO::timeout_readvall(int fd, const struct iovec* iov, int iovcnt, 
                     int timeout_ms, Notifier* intr, const char* log)
{
    struct timeval start;
    gettimeofday(&start, 0);        

    return rwvall(READV, fd, iov, iovcnt, timeout_ms, &start, intr, 
                  "timeout_readvall", log);
}

//----------------------------------------------------------------------------
int
IO::recv(int fd, char* bp, size_t len, int flags,
         Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;
    return rwdata(RECV, fd, &iov, 1, flags, -1, 0, 0, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::recvfrom(int fd, char* bp, size_t len, int flags,
             struct sockaddr* from, socklen_t* fromlen,
             Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;

    RwDataExtraArgs args;
    args.recvfrom.from    = from;
    args.recvfrom.fromlen = fromlen;
    return rwdata(RECVFROM, fd, &iov, 1, flags, -1, &args, 0, intr, 
                  false, log);
}

//----------------------------------------------------------------------------
int
IO::recvmsg(int fd, struct msghdr* msg, int flags,
            Notifier* intr, const char* log)
{
    RwDataExtraArgs args;
    args.recvmsg_hdr = msg;
    return rwdata(RECVMSG, fd, 0, 0, flags, -1, &args, 0, 
                  intr, false, log);
}


//----------------------------------------------------------------------------
int
IO::write(int fd, const char* bp, size_t len, 
          Notifier* intr, const char* log)
{
    struct iovec iov; 
    iov.iov_base = const_cast<char*>(bp);
    iov.iov_len  = len;
    return rwdata(WRITEV, fd, &iov, 1, 0, -1, 0, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::writev(int fd, const struct iovec* iov, int iovcnt, 
           Notifier* intr, const char* log)
{
    return rwdata(WRITEV, fd, iov, iovcnt, 0, -1, 0, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::writeall(int fd, const char* bp, size_t len, 
             Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = const_cast<char*>(bp);
    iov.iov_len  = len;

    return rwvall(WRITEV, fd, &iov, 1, -1, 0, intr, "writeall", log);
}

//----------------------------------------------------------------------------
int
IO::writevall(int fd, const struct iovec* iov, int iovcnt,
              Notifier* intr, const char* log)
{
    return rwvall(WRITEV, fd, iov, iovcnt, -1, 0, intr, "writevall", log);
}

//----------------------------------------------------------------------------
int 
IO::timeout_write(int fd, const char* bp, size_t len, int timeout_ms,
                  Notifier* intr, const char* log)
{
    struct iovec iov; 
    iov.iov_base = const_cast<char*>(bp);
    iov.iov_len  = len;
    return rwdata(WRITEV, fd, &iov, 1, 0, timeout_ms, 0, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int 
IO::timeout_writev(int fd, const struct iovec* iov, int iovcnt, int timeout_ms,
                   Notifier* intr, const char* log)
{
    return rwdata(WRITEV, fd, iov, iovcnt, 0, timeout_ms, 0, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int 
IO::timeout_writeall(int fd, const char* bp, size_t len, int timeout_ms,
                     Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = const_cast<char*>(bp);
    iov.iov_len  = len;

    struct timeval start;
    gettimeofday(&start, 0);    

    return rwvall(WRITEV, fd, &iov, 1, timeout_ms, &start, intr, 
                  "timeout_writeall", log);
}

//----------------------------------------------------------------------------
int 
IO::timeout_writevall(int fd, const struct iovec* iov, int iovcnt,
                      int timeout_ms, Notifier* intr, const char* log)
{
    struct timeval start;
    gettimeofday(&start, 0);    

    return rwvall(WRITEV, fd, iov, iovcnt, timeout_ms, &start, intr, 
                  "timeout_writevall", log);
}

//----------------------------------------------------------------------------
int
IO::send(int fd, const char* bp, size_t len, int flags,
         Notifier* intr, const char* log)
{    
    struct iovec iov;
    iov.iov_base = const_cast<char*>(bp);
    iov.iov_len  = len;
    return rwdata(SEND, fd, &iov, 1, flags, -1, 0, 0, intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::sendto(int fd, char* bp, size_t len, int flags,
           const struct sockaddr* to, socklen_t tolen,
           Notifier* intr, const char* log)
{
    struct iovec iov;
    iov.iov_base = bp;
    iov.iov_len  = len;

    RwDataExtraArgs args;
    args.sendto.to    = to;
    args.sendto.tolen = tolen;

    return rwdata(SENDTO, fd, &iov, 1, flags, -1, &args, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::sendmsg(int fd, const struct msghdr* msg, int flags,
            Notifier* intr, const char* log)
{
    RwDataExtraArgs args;
    args.sendmsg_hdr = msg;

    return rwdata(SENDMSG, fd, 0, 0, flags, -1, &args, 0, 
                  intr, false, log);
}

//----------------------------------------------------------------------------
int
IO::poll_single(int fd, short events, short* revents, int timeout_ms, 
                Notifier* intr, const char* log)
{   
    struct pollfd fds;
    fds.fd      = fd;
    fds.events  = events;
    
    int cc = poll_multiple(&fds, 1, timeout_ms, intr, log);
    if (revents != 0) {
        *revents = fds.revents;
    }

    return cc;
}

//----------------------------------------------------------------------------
int
IO::poll_multiple(struct pollfd* fds, int nfds, int timeout_ms,
                  Notifier* intr, const char* log)
{
    struct timeval start;
    if (timeout_ms > 0) {
        gettimeofday(&start, 0);
    }
    
    int cc = poll_with_notifier(intr, fds, nfds, timeout_ms, 
                                (timeout_ms > 0) ? &start : 0, log);
    ASSERT(cc != 0);
    if (cc > 0) {
        return cc;
    } else {
        return cc;
    }
} 

//----------------------------------------------------------------------------
int
IO::get_nonblocking(int fd, bool *nonblockingp, const char* log)
{
    int flags = 0;
    ASSERT(nonblockingp);
    
    if ((flags = fcntl(fd, F_GETFL)) < 0) {
        if (log) log_debug_p(log, "get_nonblocking: fcntl GETFL err %s",
                             strerror(errno));
        return -1;
    }

    *nonblockingp = (flags & O_NONBLOCK);
    if (log) log_debug_p(log, "get_nonblocking: %s mode",
                         *nonblockingp ? "nonblocking" : "blocking");
    return 0;
}

//----------------------------------------------------------------------------
int
IO::set_nonblocking(int fd, bool nonblocking, const char* log)
{
    int flags = 0;
    bool already = 0;
    
    if ((flags = fcntl(fd, F_GETFL)) < 0) {
        if (log) log_debug_p(log, "set_nonblocking: fcntl GETFL err %s",
                             strerror(errno));
        return -1;
    }
    
    if (nonblocking) {
        if (flags & O_NONBLOCK) {
            already = 1; // already nonblocking
            goto done;
        }
        flags = flags | O_NONBLOCK;
    } else {
        if (!(flags & O_NONBLOCK)) {
            already = 1; // already blocking
            goto done;
        }
            
        flags = flags & ~O_NONBLOCK;
    }
    
    if (fcntl(fd, F_SETFL, flags) < 0) {
        if (log) log_debug_p(log, "set_nonblocking: fcntl SETFL err %s",
                             strerror(errno));
        return -1;
    }

 done:
#ifdef OASYS_LOG_DEBUG_ENABLED
    if (log) log_debug_p(log, "set_nonblocking: %s mode %s",
                         nonblocking ? "nonblocking" : "blocking",
                         already     ? "already set" : "set");
#else
    (void) already; // prevent unused warning if debug logging is disabled
#endif

    return 0;
}

//----------------------------------------------------------------------------
int
IO::poll_with_notifier(
    Notifier*             intr,
    struct pollfd*        fds,
    size_t                nfds,
    int                   timeout,
    const struct timeval* start_time,
    const char*           log
    )
{    
    ASSERT(! (timeout > 0 && start_time == 0));
    ASSERT(timeout >= -1);

    oasys::ScratchBuffer<struct pollfd*, 
        16 * sizeof(struct pollfd)> intr_poll_set;
    struct pollfd* poll_set;
    
    if (intr == 0) {
        poll_set = fds;
    } else {
        intr_poll_set.buf(sizeof(struct pollfd) * (nfds + 1));
        poll_set = intr_poll_set.buf();
  
        for (size_t i=0; i<nfds; ++i) {
            poll_set[i].fd      = fds[i].fd;
            poll_set[i].events  = fds[i].events;
            poll_set[i].revents = 0;
        }
        poll_set[nfds].fd     = intr->read_fd();
        poll_set[nfds].events = POLLIN | POLLPRI | POLLERR;
        ++nfds;
    }

    // poll loop
 retry:
    int cc = ::poll(poll_set, nfds, timeout);
    if (cc < 0 && errno == EINTR) {
        if (timeout > 0) {
            timeout = adjust_timeout(timeout, start_time);
        }
        goto retry;
    }
    
    if (cc < 0) 
    {
        return IOERROR;
    } 
    else if (cc == 0) 
    {
        if (log) {
            log_debug_p(log, "poll_with_notifier timed out");
        }
        return IOTIMEOUT;
    } 
    else 
    {
#ifdef __APPLE__
        // there's some strange bug in the poll emulation
        if (cc > (int)nfds) {
            if (log) {
                log_warn_p(log,
                           "poll_with_notifier: returned bogus high value %d, "
                           "capping to %zu", cc, nfds);
            }
            cc = nfds;
        }
#endif

        if (log) 
        {
            StringBuffer buf;
            for (size_t i=0; i<nfds; ++i) 
            {
                buf.appendf("0x%hx ", poll_set[i].revents);
            }
            log_debug_p(log,
                        "poll_with_notifier: %d/%zu fds ready, status %s",
                        cc, nfds, buf.c_str());
        }
        
        // Always prioritize getting data before interrupt via notifier

        // XXX/demmer actually, to deal with the (rather unlikely)
        // case in which external IO events are generated faster than
        // the thread can service them, this can result in starving
        // the interrupt, which should instead take precedence, so the
        // order of the checks should be reversed. given the rarity of
        // this chance, it's a low priority item to fix
        
        bool got_event = false;
        for (size_t i=0; i<((intr != 0) ? (nfds - 1) : nfds); ++i) 
        {
            if (poll_set[i].revents & 
                (poll_set[i].events | POLLERR | POLLHUP | POLLNVAL)) 
            {
                got_event      = true;
                fds[i].revents = poll_set[i].revents;
            }
        }
        
        ASSERT(! (intr == 0 && !got_event));
        if (got_event) {
            if (log) { 
#ifdef OASYS_LOG_DEBUG_ENABLED
                logf(log, LOG_DEBUG, 
                     "poll_with_notifier: normal fd has event"); 
#endif
            }
            
            if (intr != 0 && (poll_set[nfds - 1].revents &
                              (POLLIN | POLLPRI | POLLERR)) )
            {
                ASSERT(cc > 1);
                return cc - 1;
            }
            else 
            {
                return cc;
            }
        }
        
        // We got notified
        if (intr != 0 && (poll_set[nfds - 1].revents & POLLERR))
        {
            if (log) {
                log_debug_p(log,
                            "poll_with_notifier: error in notifier fd!");
            }

            return IOERROR; // Technically this is not an error with
                            // the IO, but there should be some kind
                            // of signal here that things are not
                            // right
        } 
        else if (intr != 0 && 
                 (poll_set[nfds - 1].revents & (POLLIN | POLLPRI)) )
        {
            if (log) {
                log_debug_p(log, "poll_with_notifier: interrupted");
            }
            intr->drain_pipe(1);
            
            return IOINTR;
        }
        
        PANIC("poll_with_notifier: should not have left poll");
    }

    NOTREACHED;
}
    
//----------------------------------------------------------------------------
int 
IO::rwdata(
    IO_Op_t               op,
    int                   fd,
    const struct iovec*   iov,
    int                   iovcnt,
    int                   flags,
    int                   timeout,
    RwDataExtraArgs*      args,
    const struct timeval* start_time,
    Notifier*             intr, 
    bool                  ignore_eagain,
    const char*           log
    )
{
    ASSERT(! ((op == READV || op == WRITEV) && 
              (iov == 0 || flags != 0 || args != 0)));
    ASSERT(! ((op == RECV  || op == SEND) && 
              (iovcnt != 1 || args != 0)));
    ASSERT(! ((op == RECVFROM || op == SENDTO)  && 
              (iovcnt != 1 || args == 0)));
    ASSERT(! ((op == RECVMSG || op == SENDMSG) && 
              (iov != 0 && args == 0)));
    ASSERT(timeout >= -1);
    ASSERT(! (timeout > -1 && start_time == 0));

    struct pollfd poll_fd;
    poll_fd.fd = fd;
    switch (op) {
    case READV: case RECV: case RECVFROM: case RECVMSG:
        poll_fd.events = POLLIN | POLLPRI; 
        break;
    case WRITEV: case SEND: case SENDTO: case SENDMSG:
        poll_fd.events = POLLOUT; 
        break;
    default:
        PANIC("Unknown IO type");
    }
   
    int cc;
    while (true) 
    {
        if (intr || timeout > -1) {
            cc = poll_with_notifier(intr, &poll_fd, 1, timeout, 
                                    start_time, log);
            if (cc == IOERROR || cc == IOTIMEOUT || cc == IOINTR) {
                return cc;
            } 
        }

        switch (op) {
        case READV:
            cc = ::readv(fd, iov, iovcnt);
            if (log) log_debug_p(log, "::readv() fd %d cc %d", fd, cc);
            break;
        case RECV:
            cc = ::recv(fd, iov[0].iov_base, iov[0].iov_len, flags);
            if (log) log_debug_p(log, "::recv() fd %d %p/%zu cc %d", 
                                 fd, iov[0].iov_base, iov[0].iov_len, cc);
            break;
        case RECVFROM:
            cc = ::recvfrom(fd, iov[0].iov_base, iov[0].iov_len, flags,
                            args->recvfrom.from, 
                            args->recvfrom.fromlen);
            if (log) log_debug_p(log, "::recvfrom() fd %d %p/%zu cc %d", 
                                 fd, iov[0].iov_base, iov[0].iov_len, cc);
            break;
        case RECVMSG:
            cc = ::recvmsg(fd, args->recvmsg_hdr, flags);
            if (log) log_debug_p(log, "::recvmsg() fd %d %p cc %d", 
                                 fd, args->recvmsg_hdr, cc);
            break;
        case WRITEV:
            cc = ::writev(fd, iov, iovcnt);
            if (log) log_debug_p(log, "::writev() fd %d cc %d", fd, cc);
            break;
        case SEND:
            cc = ::send(fd, iov[0].iov_base, iov[0].iov_len, flags);
            if (log) log_debug_p(log, "::send() fd %d %p/%zu cc %d", 
                                 fd, iov[0].iov_base, iov[0].iov_len, cc);
            break;
        case SENDTO:
            cc = ::sendto(fd, iov[0].iov_base, iov[0].iov_len, flags,
                          args->sendto.to, args->sendto.tolen);
            if (log) log_debug_p(log, "::sendto() fd %d %p/%zu cc %d", 
                                 fd, iov[0].iov_base, iov[0].iov_len, cc);
            break;
        case SENDMSG:
            cc = ::sendmsg(fd, args->sendmsg_hdr, flags);
            if (log) log_debug_p(log, "::sendmsg() fd %d %p cc %d", 
                                 fd, args->sendmsg_hdr, cc);
            break;
        default:
            PANIC("Unknown IO type");
        }
        
        if (cc < 0 && 
            ( (errno == EAGAIN && ignore_eagain) || errno == EINTR) ) 
        {
            if (timeout > 0) {
                timeout = adjust_timeout(timeout, start_time);
            }
            continue;
        }

        if (cc < 0) {
            if (errno == EAGAIN) {
                return IOAGAIN;
            } else {
                return IOERROR;
            }
        } else if (cc == 0) {
            return IOEOF;
        } else {
            return cc;
        }
    } // while (true)

    NOTREACHED;
}

//----------------------------------------------------------------------------
int
IO::rwvall(
    IO_Op_t               op,
    int                   fd,
    const struct iovec*   iov, 
    int                   iovcnt,
    int                   timeout,             
    const struct timeval* start,
    Notifier*             intr,
    const char*           fcn_name,
    const char*           log
    )
{
    (void)fcn_name;
    ASSERT(op == READV || op == WRITEV);
    ASSERT(! (timeout != -1 && start == 0));

    COWIoVec cow_iov(iov, iovcnt);
    int total_bytes = cow_iov.bytes_left();

    while (cow_iov.bytes_left() > 0) {
        int cc = rwdata(op, fd, cow_iov.iov(), cow_iov.iovcnt(), 
                        0, timeout, 0, start, intr, true, log);
        if (cc < 0) {
            if (log && cc != IOTIMEOUT && cc != IOINTR) {
                log_debug_p(log, "%s %s %s", 
                            fcn_name, ioerr2str(cc), strerror(errno));
            }
            return cc;
        } else if (cc == 0) {
            if (log) {
                log_debug_p(log, "%s eof", fcn_name);
            }
            // XXX/demmer this is a strange case since we may have
            // actually read/written some amount before getting the
            // eof... 
            return IOEOF;
        } else {
            cow_iov.consume(cc);
            if (log) {
                log_debug_p(log, "%s %d bytes %zu left %d total",
                            fcn_name, cc, cow_iov.bytes_left(), total_bytes);
            }
            
            if (timeout > 0) {
                timeout = adjust_timeout(timeout, start);
            }
        }
    }

    return total_bytes;
}

//----------------------------------------------------------------------------
int
IO::adjust_timeout(int timeout, const struct timeval* start)
{
    struct timeval now;
    int err = gettimeofday(&now, 0);
    ASSERT(err == 0);
    
    now.tv_sec  -= start->tv_sec;
    now.tv_usec -= start->tv_usec;
    timeout -= now.tv_sec * 1000 + now.tv_usec / 1000;
    if (timeout < 0) {
        timeout = 0;
    }

    return timeout;
}

} // namespace oasys
