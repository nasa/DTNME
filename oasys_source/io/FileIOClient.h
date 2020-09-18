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


#ifndef _OASYS_FILE_IOCLIENT_H_
#define _OASYS_FILE_IOCLIENT_H_

#include "FdIOClient.h"
#include <fcntl.h>
#include <string.h>

namespace oasys {

/**
 * IOClient derivative for real files -- not sockets. Unlike the base
 * class FdIOClient, FileIOClient contains the path as a member
 * variable and exposes file specific system calls, i.e. open(),
 * lseek(), etc.
 */
class FileIOClient : public FdIOClient {
public:
    /// Basic constructor, leaves both path and fd unset
    FileIOClient(const char* logpath = "/oasys/io/FileIOClient");
    virtual ~FileIOClient();

    ///@{
    /// System call wrappers
    virtual int open(const char* path, int flags, int* errnop = 0);
    virtual int open(const char* path, int flags, mode_t mode, int* errnop = 0);
    virtual int close();
    virtual int unlink();
    virtual int lseek(off_t offset, int whence);
    virtual int truncate(off_t length);
    virtual int mkstemp(char* temp);
    virtual int stat(struct stat* buf);
    virtual int lstat(struct stat* buf);
    ///@}

    /// Copy the contents of the current file to the given destination
    /// file. If len is non-zero, copy at most that many bytes,
    /// otherwise copy the whole file.
    int copy_contents(FileIOClient* dest, size_t len = 0);

    /// Set the path associated with this file handle
    void set_path(const std::string& path) {
        path_ = path;
    }

    /// Reopen a previously opened path
    int reopen(int flags, int mode = 0);

    /// Check if the file descriptor is open
    bool is_open() const { return fd_ != -1; }

    /// Path accessor
    const char* path() const { return path_.c_str(); }

    /// Path accessor
    const std::string& path_str() const { return path_; }

    /// Path accessor
    size_t path_len() const { return path_.length(); }

protected:
    /// Path to the file
    std::string path_;
};

} // namespace oasys

#endif /* _OASYS_FILE_IOCLIENT_H_ */
