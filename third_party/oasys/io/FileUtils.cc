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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include "debug/Log.h"
#include "FileUtils.h"

namespace oasys {

//------------------------------------------------------------------
bool
FileUtils::readable(const char* path, const char* log)
{
    struct stat st;
    int ret = stat(path, &st);

    if (ret == -1) {
        logf(log, LOG_DEBUG,
             "FileUtils::readable(%s): error running stat %s",
             path, strerror(errno));
        return false;
    }

    if (!S_ISREG(st.st_mode) &&
        !S_ISBLK(st.st_mode) &&
        !S_ISCHR(st.st_mode))
    {
        logf(log, LOG_DEBUG,
             "FileUtils::readable(%s): not a regular file or device", path);
        return false;
    }

    if ((st.st_mode & S_IRUSR) == 0) {
        logf(log, LOG_DEBUG,
             "FileUtils::readable(%s): no readable permissions", path);
        return false;
    }
    
    return true;
}

//------------------------------------------------------------------
int
FileUtils::size(const char* path, const char* log)
{
    struct stat st;
    int ret = stat(path, &st);

    if (ret == -1) {
        if (log) {
            logf(log, LOG_DEBUG,
                 "FileUtils::size(%s): error running stat %s",
                 path, strerror(errno));
        }
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        if (log) {
            logf(log, LOG_DEBUG,
                 "FileUtils::size(%s): not a regular file", path);
        }
        return -1;
    }
    
    return st.st_size;
}

//------------------------------------------------------------------
void
FileUtils::abspath(std::string* path)
{
    if ((*path)[0] != '/') {
        char cwd[PATH_MAX];
        ::getcwd(cwd, PATH_MAX);

        std::string temp = *path;
        *path = cwd;
        *path += '/' + temp;
    }
}

//------------------------------------------------------------------
int
FileUtils::rm_all_from_dir(const char* path, bool recursive)
{
    DIR* dir = opendir(path);
    
    if (dir == 0) {
        return errno;
    }

    struct dirent* ent = readdir(dir);
    if (ent == 0) {
        return errno;
    }
    
    std::string dot(".");
    std::string dotdot("..");

    while (ent != 0) {
        // skip the directories . and ..
        if (dot == ent->d_name || dotdot == ent->d_name) {
            ent = readdir(dir);
            continue;
        }

        std::string ent_name(path);
        ent_name = ent_name + "/" + ent->d_name;

        bool is_dir = false;

#if defined(DT_DIR)
        is_dir = (ent->d_type == DT_DIR);
#else
        struct stat ss;
        stat(ent->d_name, &ss);
        is_dir = S_ISDIR(ss.st_mode);
#endif

        if (recursive && is_dir) {
            rm_all_from_dir(ent_name.c_str(), true);
            rmdir(ent_name.c_str());
        }
        
        else {
            unlink(ent_name.c_str());
        }
    
        ent = readdir(dir);
    }
    
    closedir(dir);

    return 0;
}

//------------------------------------------------------------------
int
FileUtils::fast_copy(const char* src_filename, const char* dest_filename)
{
    int src_fd = open(src_filename, O_RDONLY);
    if (src_fd == -1) 
    {
        return -1;
    }

    int dest_fd = open(dest_filename, 
                       O_WRONLY | O_CREAT | O_EXCL,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dest_fd == -1)
    {
        close(src_fd);
        return -1;
    }

    struct stat stat_buf;
    int err = fstat(src_fd, &stat_buf);
    ASSERT(err != -1);
    
    const size_t BUFSIZE = 1024 * 8;
    char buf[BUFSIZE];
    int cc = 0;
    do {
        cc = read(src_fd, buf, BUFSIZE);

        int dd = write(dest_fd, buf, cc);
        ASSERT(dd == cc);
    } while (cc > 0);
        
    /*
      GRRR -- stupid Linus disabled sendfile between things that are
      not sockets even though the man page doesn't say anything about
      that.

      off_t offset = 0;
      ssize_t total_bytes = sendfile(dest_fd, src_fd, &offset, stat_buf.st_size);
      ASSERTF(total_bytes == stat_buf.st_size, "error %s", strerror(errno));
    */    

    close(src_fd);
    close(dest_fd);

    return 0;
}

//----------------------------------------------------------------------------
int 
FileUtils::StatFormat::format(char* buf, size_t len) const
{
    return snprintf(buf, len, 
                    "dev %u mode %4o nlink %u "
                    "uid %u gid %u type %u "
                    "size %zu at %u mt %u "
                    "ct %u",
                    static_cast<unsigned int>(stat_.st_dev),
                    static_cast<unsigned int>(stat_.st_mode),
                    static_cast<unsigned int>(stat_.st_nlink),
                    static_cast<unsigned int>(stat_.st_uid),
                    static_cast<unsigned int>(stat_.st_gid),
                    static_cast<unsigned int>(stat_.st_rdev),
                    static_cast<size_t>      (stat_.st_size),
                    static_cast<unsigned int>(stat_.st_atime),
                    static_cast<unsigned int>(stat_.st_mtime),
                    static_cast<unsigned int>(stat_.st_ctime));
}

} // end namespace
