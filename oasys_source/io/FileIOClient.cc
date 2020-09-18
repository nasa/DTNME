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
#include "FileIOClient.h"
#include "IO.h"

namespace oasys {

//----------------------------------------------------------------------
FileIOClient::FileIOClient(const char* logpath)
    : FdIOClient(-1, NULL, logpath)
{
}

//----------------------------------------------------------------------
FileIOClient::~FileIOClient()
{
    if (fd_ != -1)
        close();
}

//----------------------------------------------------------------------
int
FileIOClient::open(const char* path, int flags, int* errnop)
{
    path_.assign(path);
    fd_ = IO::open(path, flags, errnop, logpath_);
    return fd_;
}

//----------------------------------------------------------------------
int
FileIOClient::open(const char* path, int flags, mode_t mode, int* errnop)
{
    path_.assign(path);
    fd_ = IO::open(path, flags, mode, errnop, logpath_);
    return fd_;
}

//----------------------------------------------------------------------
int
FileIOClient::close()
{
    int ret = IO::close(fd_, logpath_, path_.c_str());
    fd_ = -1;
    return ret;
}

//----------------------------------------------------------------------
int
FileIOClient::reopen(int flags, int mode)
{
    ASSERT(path_.length() != 0);
    fd_ = IO::open(path_.c_str(), flags, mode, NULL, logpath_);
    return fd_;
}

//----------------------------------------------------------------------
int
FileIOClient::unlink()
{
    if (path_.length() == 0)
        return 0;
    
    int ret = 0;
    ret = IO::unlink(path_.c_str(), logpath_);
    path_.assign("");
    return ret;
}

//----------------------------------------------------------------------
int
FileIOClient::lseek(off_t offset, int whence)
{
    return IO::lseek(fd_, offset, whence, logpath_);
}

//----------------------------------------------------------------------
int
FileIOClient::truncate(off_t length)
{
    return IO::truncate(fd_, length, logpath_);
}

//----------------------------------------------------------------------
int
FileIOClient::mkstemp(char* temp)
{
    if (fd_ != -1) {
        log_err("can't call mkstemp on open file");
        return -1;
    }

    fd_ = IO::mkstemp(temp, logpath_);

    path_.assign(temp);
    return fd_;
}

//----------------------------------------------------------------------
int
FileIOClient::stat(struct stat* buf)
{
    return IO::stat(path_.c_str(), buf, logpath_);
}

//----------------------------------------------------------------------
int
FileIOClient::lstat(struct stat* buf)
{
    return IO::lstat(path_.c_str(), buf, logpath_);
}

//----------------------------------------------------------------------
int
FileIOClient::copy_contents(FileIOClient* dest, size_t len)
{
    char buf[4096];
    int n, cc;
    int origlen = len;
    int total = 0;

    while (1) {
        if (origlen == 0) {
            n = sizeof(buf);
        } else {
            n = std::min(len, sizeof(buf));
        }

        cc = read(buf, n);
        if (cc < 0) {
            log_err("copy_contents: error reading %d bytes: %s",
                    n, strerror(errno));
            return -1;
        }

        if (cc == 0) {
            if (origlen != 0 && len != 0) {
                log_err("copy_contents: file %s too short (expected %d bytes)",
                        path_.c_str(), origlen);
                return -1;
            }

            break;
        }
        
        if (dest->writeall(buf, cc) != cc) {
            log_err("copy_contents: error writing %d bytes: %s",
                    cc, strerror(errno));
            return -1;
        }

        total += cc;
        
        if (origlen != 0) {
            len -= cc;
            if (len == 0) {
                break;
            }
        }
    }
    
    return total;
}

} // namespace oasys
