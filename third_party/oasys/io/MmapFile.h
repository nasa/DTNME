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

#ifndef _MMAPFILE_H_
#define _MMAPFILE_H_

#include <sys/types.h>
#include <sys/mman.h>
#include "../debug/Log.h"

namespace oasys {

/**
 * Simple wrapper class for a memory-mapped file that encapsulates the
 * file descriptor and the memory mapping.
 */
class MmapFile : public Logger {
public:
    /**
     * Constructor initializes the object to empty.
     */
    MmapFile(const char* logpath);

    /**
     * Destructor unmaps and closes the file.
     */
    virtual ~MmapFile();

    /**
     * Sets up a file-based mmap.
     *
     * @return the mapping pointer or NULL if there's an error
     */
    void* map(const char* filename, int prot, int flags, 
              size_t len = 0, off_t offset = 0);
    
    /**
     * Unmaps the current mapping (if any).
     *
     * @returns true if successful
     */
    bool unmap();

    /// @{ Accessors
    void*  ptr() { return ptr_; }
    size_t len() { return len_; }
    /// @}

private:
    /// The mmap pointer
    void* ptr_;

    /// The length of the mapping
    size_t len_;
};

} // namespace oasys


#endif /* _MMAPFILE_H_ */
