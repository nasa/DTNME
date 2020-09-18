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


#ifndef _OASYS_FILE_UTILS_H_
#define _OASYS_FILE_UTILS_H_

#include <string>
#include <sys/stat.h>

#include "../debug/Formatter.h"

namespace oasys {

/**
 * Abstraction class for some stateless file operations such as
 * probing to see if a file and/or directory exists, is readable, etc.
 *
 * For most operations, this is just a simpler interface to the stat()
 * system call.
 */
class FileUtils {
public:
    static bool readable(const char* path,
                         const char* log = 0);

    /// Return the size of the file or -1 on error
    static int size(const char* path,
                    const char* log = 0);

    /// Make sure the given path is absolute, prepending the current
    /// directory if necessary.
    static void abspath(std::string* path);

    /// Deletes all of the files from a given directory
    /// @return 0 on success, errno otherwise
    static int rm_all_from_dir(const char* path, bool recursive = false);
    
    /*!
     * Copy a file as fast as possible to dest.
     *
     * @return -1 if the src cannot be opened or the dest cannot be
     * created exclusively.
     */
    static int fast_copy(const char* src_filename, const char* dest_filename);

    struct StatFormat : public Formatter {
        StatFormat(const struct stat& stat)
            : stat_(stat) {}

        int format(char* buf, size_t len) const;

        struct stat stat_;
    };
};

}

#endif /* _OASYS_FILE_UTILS_H_ */
