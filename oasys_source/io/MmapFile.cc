/*
 *    Copyright 2006 Intel Corporation
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

#include "MmapFile.h"

#include <cerrno>
#include <fcntl.h>

#include "FileIOClient.h"
#include "FileUtils.h"

namespace oasys {

//----------------------------------------------------------------------
MmapFile::MmapFile(const char* logpath)
    : Logger("MmapFile", "%s", logpath),
      ptr_(NULL),
      len_(0)
{
}

//----------------------------------------------------------------------
MmapFile::~MmapFile()
{
    if (ptr_ != NULL)
        unmap();
}

//----------------------------------------------------------------------
void*
MmapFile::map(const char* filename, int prot, int flags, 
              size_t len, off_t offset)
{
    if (len == 0) {
        int ret = FileUtils::size(filename, logpath_);
        if (ret < 0) {
            log_err("error getting size of file '%s': %s",
                    filename, strerror(errno));
            return NULL;
        }
        len = (size_t)ret;
    }

    ASSERT(ptr_ == NULL);
    ASSERT(offset < (int)len);

    FileIOClient f;
    f.logpathf("%s/file", logpath_);

    int open_flags = 0;
    if (prot & PROT_READ)  open_flags |= O_RDONLY;
    if (prot & PROT_WRITE) open_flags |= O_WRONLY;

    int err;
    int fd = f.open(filename, open_flags, &err);
    if (fd < 0) {
        log_err("error opening file '%s': %s",
                filename, strerror(err));
        return NULL;
    }

    len_ = len;
    ptr_ = mmap(0, len, prot, flags, fd, offset);
    if (ptr_ == (void*)-1) {
        log_err("error in mmap of file '%s' (len %zu offset %llu): %s",
                filename, len, U64FMT(offset), strerror(errno));
        ptr_ = NULL;
        len_ = 0;
        return NULL;
    }

    return ptr_;
}

//----------------------------------------------------------------------
bool
MmapFile::unmap()
{
    ASSERT(ptr_ != NULL);
    int ret = munmap(ptr_, len_);
    if (ret != 0) {
        log_err("error in munmap: %s", strerror(errno));
        return false;
    }
    ptr_ = NULL;
    len_ = 0;
    return true;
}

} // namespace oasys
