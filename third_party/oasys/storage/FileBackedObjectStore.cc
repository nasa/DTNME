#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include "../io/FileUtils.h"
#include "FileBackedObjectStore.h"

namespace oasys {

//----------------------------------------------------------------------------
FileBackedObjectStore::FileBackedObjectStore(const std::string& root)
    : Logger("FileBackedObjectStore", "/store/file-backed"),
      root_(root)
{
    // check that the root directory is readable
    struct stat dir_stat;
    int err = stat(root_.c_str(), &dir_stat);
    if (err != 0 && errno == ENOENT)
    {
        log_info("Root directory %s not found, attempting to create.", 
                 root.c_str());
        char cmd[256];
        snprintf(cmd, 256, "mkdir -p %s", root.c_str());
        system(cmd);
        
        err = stat(root_.c_str(), &dir_stat);
    }
    
    ASSERTF(err == 0, "Can't stat root %s, error=%s",
            root_.c_str(), strerror(errno));
    ASSERTF(dir_stat.st_mode & S_IRWXU, 
            "%s must have rwx permissions.", root_.c_str());
    
    logpathf("/store/file-backed/%s", root.c_str());
}

//----------------------------------------------------------------------------
int 
FileBackedObjectStore::new_object(const std::string& key)
{
    if (object_exists(key))
    {
        return -1;
    }
    
    int fd = open(object_path(key).c_str(), 
                  O_WRONLY | O_CREAT | O_EXCL,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ASSERT(fd != -1);
    close(fd);

    return 0;
}
    
//----------------------------------------------------------------------------
FileBackedObjectHandle
FileBackedObjectStore::get_handle(const std::string& key, int flags)
{
    ASSERT(object_exists(key));

    return FileBackedObjectHandle(
        new FileBackedObject(object_path(key), flags));
}
    
//----------------------------------------------------------------------------
bool 
FileBackedObjectStore::object_exists(const std::string& key)
{
    struct stat stat_buf;
    int err = stat(object_path(key).c_str(), &stat_buf);
    if (err == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

//----------------------------------------------------------------------------
int 
FileBackedObjectStore::del_object(const std::string& key)
{
    if (! object_exists(key))
    {
        return -1;
    }

    int err = unlink(object_path(key).c_str());
    ASSERT(err == 0);
    
    return 0;
}

//----------------------------------------------------------------------------
int 
FileBackedObjectStore::copy_object(const std::string& src,  
                                   const std::string& dest)
{
    std::string src_path, dest_path;

    if (! object_exists(src))
    {
        log_debug("src %s doesn't exist, not copying", src.c_str());
        return -1;
    }
    
    if (object_exists(dest))
    {
        log_debug("dest %s exists, not copying", dest.c_str());
        return -1;
    }

    int err = FileUtils::fast_copy(object_path(src).c_str(),
                                   object_path(dest).c_str());
    ASSERT(err != -1);
    return 0;
}

//----------------------------------------------------------------------------
FileBackedObjectStore::Stats
FileBackedObjectStore::get_stats() const
{
    Stats stats;

    DIR* dir = opendir(root_.c_str());
    ASSERT(dir != 0);
    struct dirent* ent;
    do {
        ent = readdir(dir);
        ++stats.total_objects_;
    } while (ent != 0);
    closedir(dir);

    return stats;
}

//----------------------------------------------------------------------------
void 
FileBackedObjectStore::get_object_names(std::vector<std::string>* names)
{
    DIR* dir = opendir(root_.c_str());
    struct dirent* dirent;

    names->clear();
    do {
        dirent = readdir(dir);
        if (dirent != 0)
        {
            if (! (strcmp(dirent->d_name, ".")  == 0 ||
                   (strcmp(dirent->d_name, "..") == 0)))
            {
                names->push_back(dirent->d_name);
            }
        }
    } while(dirent != 0);

    closedir(dir);
}

//----------------------------------------------------------------------------
std::string
FileBackedObjectStore::object_path(const std::string& key)
{
    std::string path = root_;
    path.append("/");
    path.append(key);

    return path;
}

//----------------------------------------------------------------------------
FileBackedObjectStore::Stats::Stats()
    : total_objects_(0),
      open_handles_(0)
{}

} // namespace oasys
