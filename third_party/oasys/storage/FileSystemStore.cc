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

#include "FileSystemStore.h"
#include "StorageConfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include "../util/ExpandableBuffer.h"
#include "../serialize/KeySerialize.h"
#include "../serialize/TypeCollection.h"
#include "../io/FileUtils.h"
#include "../io/IO.h"


namespace oasys {

//----------------------------------------------------------------------------
FileSystemStore::FileSystemStore(const char* logpath)
    : DurableStoreImpl("FileSystemStore", logpath),
      db_dir_("INVALID"),
      tables_dir_("INVALID"),
      default_perm_(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP),
      fd_cache_(0)
{}

//----------------------------------------------------------------------------
FileSystemStore::~FileSystemStore()
{}

//----------------------------------------------------------------------------
int 
FileSystemStore::init(const StorageConfig& cfg)
{
    if (cfg.dbdir_ == "") {
        return -1;
    }

    if (cfg.dbname_ == "") {
        return -1;
    }
    
    db_dir_ = cfg.dbdir_;
    FileUtils::abspath(&db_dir_);

    tables_dir_ = db_dir_ + "/" + cfg.dbname_;

    bool tidy = cfg.tidy_;
    bool init = cfg.init_;

    // Always regenerate the directories if we are going to be
    // deleting them anyways
    if (tidy) {
        init = true;
    }

    if (init && tidy) 
    {
        if (check_database() == 0) {
            tidy_database();
        }
        int err = init_database();
        if (err != 0) {
            return -1;
        }
    }
    else if (init && ! tidy) 
    {
        if (check_database() == -2) {
            int err = init_database();
            if (err != 0) {
                return -1;
            }
        }
    }
    else 
    {
        if (check_database() != 0) {
            log_err("Database directory not found");
            return -1;
        }
    }

    if (cfg.fs_fd_cache_size_ > 0) {
        fd_cache_ = new FdCache(logpath_, cfg.fs_fd_cache_size_);
    }

    log_info("init() done");
    init_ = true;

    return 0;
}

//----------------------------------------------------------------------------
int
FileSystemStore::end_transaction(void *txid, bool be_durable)
{
    (void) txid;

    log_debug("FileSystemStore::end_transaction %p enter.", txid);

    if (be_durable)
    {
        fd_cache_->sync_all();
    }

    return DS_OK;
}

//----------------------------------------------------------------------------
int 
FileSystemStore::get_table(DurableTableImpl** table,
                           const std::string& name,
                           int                flags,
                           PrototypeVector&   prototypes)
{
    (void)prototypes;
    
    ASSERT(init_);

    std::string dir_path =  tables_dir_;
    dir_path.append("/");
    dir_path.append(name);

    struct stat st;
    int err = stat(dir_path.c_str(), &st);
    
    // Already existing directory
    if (err != 0 && errno == ENOENT) {
        if ( ! (flags & DS_CREATE)) {
            return DS_NOTFOUND;
        }
        
        int err = mkdir(dir_path.c_str(), default_perm_);
        if (err != 0) {
            err = errno;
            log_err("Couldn't mkdir: %s", strerror(err));
            return DS_ERR;
        }
    } else if (err != 0) { 
        return DS_ERR;
    } else if (err == 0 && (flags & DS_EXCL)) {
        return DS_EXISTS;
    }
    
    FileSystemTable* table_ptr =
        new FileSystemTable(logpath_, 
                            name, 
                            dir_path, 
                            flags & DS_MULTITYPE, 
                            fd_cache_);
    ASSERT(table_ptr);
    
    *table = table_ptr;

    return 0;
}

//----------------------------------------------------------------------------
int 
FileSystemStore::del_table(const std::string& name)
{
    // XXX/bowei -- don't handle table sharing for now
    ASSERT(init_);

    std::string dir_path = tables_dir_;
    dir_path.append("/");
    dir_path.append(name);
    
    FileUtils::rm_all_from_dir(dir_path.c_str());

    // clean out the directory
    int err;
    err = rmdir(dir_path.c_str());
    if (err != 0) {
        log_warn("couldn't remove directory, %s", strerror(errno));
        return -1;
    }

    return 0; 
}

//----------------------------------------------------------------------------
int 
FileSystemStore::get_table_names(StringVector* names)
{
    names->clear();
    
    DIR* dir = opendir(tables_dir_.c_str());
    if (dir == 0) {
        log_err("Can't get table names from directory");
        return DS_ERR;
    }

    struct dirent* ent = readdir(dir);
    while (ent != 0) {
        names->push_back(ent->d_name);
        ent = readdir(dir);
    }

    closedir(dir);

    return 0;
}

//----------------------------------------------------------------------------
std::string 
FileSystemStore::get_info() const
{
    StringBuffer desc;

    return "FileSystemStore";
}

//----------------------------------------------------------------------------
int 
FileSystemStore::check_database()
{
    DIR* dir = opendir(tables_dir_.c_str());
    if (dir == 0) {
        if (errno == ENOENT) {
            return -2;
        } else {
            return -1;
        }
    }
    closedir(dir);

    return 0;
}

//----------------------------------------------------------------------------
int 
FileSystemStore::init_database()
{
    log_notice("init database (tables dir '%s'", tables_dir_.c_str());

    int err = mkdir(db_dir_.c_str(), default_perm_);
    if (err != 0 && errno != EEXIST) {
        log_warn("init() failed: %s", strerror(errno));
        return -1;
    }

    err = mkdir(tables_dir_.c_str(), default_perm_);
    if (err != 0 && errno != EEXIST) {
        log_warn("init() failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------------
void
FileSystemStore::tidy_database()
{
    log_notice("Tidy() database, rm -rf %s", db_dir_.c_str());
    
    char cmd[256];
    int cc = snprintf(cmd, 256, "rm -rf %s", db_dir_.c_str());
    ASSERT(cc < 256);
    system(cmd);
}

//----------------------------------------------------------------------------
FileSystemTable::FileSystemTable(const char*               logpath,
                                 const std::string&        table_name,
                                 const std::string&        path,
                                 bool                      multitype,
                                 FileSystemStore::FdCache* cache)
    : DurableTableImpl(table_name, multitype),
      Logger("FileSystemTable", "%s/%s", logpath, table_name.c_str()),
      path_(path),
      cache_(cache)
{}

//----------------------------------------------------------------------
FileSystemTable::~FileSystemTable()
{}

//----------------------------------------------------------------------------
int 
FileSystemTable::get(const SerializableObject& key,
                     SerializableObject*       data)
{
    ASSERTF(!multitype_, "single-type get called for multi-type table");

    ScratchBuffer<u_char*, 4096> buf;
    int err = get_common(key, &buf);
    if (err != 0) {
        return err;
    }
    
    Unmarshal um(Serialize::CONTEXT_LOCAL, buf.buf(), buf.len());
    err = um.action(data);
    if (err != 0) {
        return DS_ERR;
    }

    return 0;
}
    
//----------------------------------------------------------------------------
int 
FileSystemTable::get(const SerializableObject&   key,
                     SerializableObject**        data,
                     TypeCollection::Allocator_t allocator)
{
    ASSERTF(multitype_, "multi-type get called for single-type table");

    ScratchBuffer<u_char*, 4096> buf;
    int err = get_common(key, &buf);
    if (err != 0) {
        return err;
    }
    
    Unmarshal um(Serialize::CONTEXT_LOCAL, buf.buf(), buf.len());

    TypeCollection::TypeCode_t typecode;
    um.process("typecode", &typecode);
    
    err = allocator(typecode, data);
    if (err != 0) {
        return DS_ERR;
    }
    err = um.action(*data);
    if (err != 0) {
        return DS_ERR;
    }

    return 0;
}
    
//----------------------------------------------------------------------------
int 
FileSystemTable::put(const SerializableObject&  key,
                     TypeCollection::TypeCode_t typecode,
                     const SerializableObject*  data,
                     int flags)
{
    ScratchBuffer<char*, 512> key_str;
    KeyMarshal s_key(&key_str, "-");

    if (s_key.action(&key) != 0) {
        log_err("Can't get key");
        return DS_ERR;
    }
    
    ScratchBuffer<u_char*, 4096> scratch;
    Marshal m(Serialize::CONTEXT_LOCAL, &scratch);
    
    if (multitype_) {
        m.process("typecode", &typecode);
    }
    if (m.action(data) != 0) {
        log_warn("can't marshal data");
        return DS_ERR;
    }

    std::string filename = path_ + "/" + key_str.buf();
    int data_elt_fd      = -1;
    int open_flags       = O_TRUNC | O_RDWR;

    if (flags & DS_EXCL) {
        open_flags |= O_EXCL;       
    }
    
    if (flags & DS_CREATE) {
        open_flags |= O_CREAT;
    }
    
    log_debug("opening file %s", filename.c_str());
    
    if (cache_) 
    {
        data_elt_fd = cache_->get_and_pin(filename);
    }

    if (data_elt_fd == -1) 
    {
        data_elt_fd = open(filename.c_str(), open_flags, 
                           S_IRUSR | S_IWUSR | S_IRGRP);
        if (data_elt_fd == -1)
        {
            if (errno == ENOENT) 
            {
                ASSERT(! (flags      & DS_CREATE));
                ASSERT(! (open_flags & O_CREAT));
                log_debug("file not found and DS_CREATE not specified");
                return DS_NOTFOUND;
            } 
            else if (errno == EEXIST) 
            {
                ASSERT(open_flags & O_EXCL);
                log_debug("file found and DS_EXCL specified");
                return DS_EXISTS;
            } 
            else 
            {
                log_warn("can't open %s: %s", 
                         filename.c_str(), strerror(errno));
                return DS_ERR;
            }
        }

        if (cache_) 
        {
            int old_fd = data_elt_fd;
            data_elt_fd = cache_->put_and_pin(filename, data_elt_fd);
            if (old_fd != data_elt_fd) 
            {
                IO::close(old_fd);
            }
        }
    } 
    else if (cache_ && (flags & DS_EXCL))
    {
        cache_->unpin(filename);
        return DS_EXISTS;
    }

    log_debug("created file %s, fd = %d", 
              filename.c_str(), data_elt_fd);
    
    if (cache_) 
    {
        int cc = IO::lseek(data_elt_fd, 0, SEEK_SET);
        ASSERT(cc == 0);
    }

    int cc = IO::writeall(data_elt_fd, 
                          reinterpret_cast<char*>(scratch.buf()), 
                          scratch.len());
    if (cc != static_cast<int>(scratch.len())) 
    {
        log_warn("put() - errors writing to file %s, %d: %s",
                 filename.c_str(), cc, strerror(errno));
        if (cache_) 
        {
            cache_->unpin(filename);
        }
        return DS_ERR;
    }
    
    if (cache_)
    {
        // close(data_elt_fd); XXX/bowei -- fsync?
        cache_->unpin(filename);
    }
    else
    {
        IO::close(data_elt_fd);
    }

    return 0;
}
    
//----------------------------------------------------------------------------
int 
FileSystemTable::del(const SerializableObject& key)
{
    ScratchBuffer<char*, 512> key_str;
    KeyMarshal s_key(&key_str, "-");

    if (s_key.action(&key) != 0) {
        log_err("Can't get key");
        return DS_ERR;
    }
    
    std::string filename = path_ + "/" + key_str.buf();

    if (cache_)
    {
        cache_->close(filename);
    }

    int err = unlink(filename.c_str());
    if (err == -1) 
    {
        if (errno == ENOENT) {
            return DS_NOTFOUND;
        }
        
        log_warn("can't unlink file %s - %s", filename.c_str(),
                 strerror(errno));
        return DS_ERR;
    }
    
    return 0;
}

//----------------------------------------------------------------------------
size_t 
FileSystemTable::size() const
{
    // XXX/bowei -- be inefficient for now
    DIR* dir = opendir(path_.c_str());
    ASSERT(dir != 0);
    
    size_t count;
    struct dirent* ent;

    for (count = 0, ent = readdir(dir); 
         ent != 0; ent = readdir(dir))
    {
        ASSERT(ent != 0);
        ++count;
    }

    closedir(dir);

    // count always includes '.' and '..'
    log_debug("table size = %zu", count - 2);

    return count - 2; 
}
    
//----------------------------------------------------------------------------
DurableIterator* 
FileSystemTable::itr()
{
    return new FileSystemIterator(path_);
}

//----------------------------------------------------------------------------
int 
FileSystemTable::get_common(const SerializableObject& key,
                            ExpandableBuffer*         buf)
{
    ScratchBuffer<char*, 512> key_str;
    KeyMarshal serialize(&key_str, "-");
    if (serialize.action(&key) != 0) {
        log_err("Can't get key");
        return DS_ERR;
    }
    
    std::string file_name(key_str.at(0));
    std::string file_path = path_ + "/" + file_name;
    log_debug("opening file %s", file_path.c_str());

    
    int fd = -1;
    if (cache_) 
    {
        fd = cache_->get_and_pin(file_path);
    }

    if (fd == -1) {
        fd = open(file_path.c_str(), O_RDWR);
        if (fd == -1) 
        {
            log_debug("error opening file %s: %s",
                      file_path.c_str(), strerror(errno));
            if (errno == ENOENT) 
            {
                return DS_NOTFOUND;
            }
            
            return DS_ERR;
        }

        if (cache_) 
        {
            int old_fd = fd;
            fd = cache_->put_and_pin(file_path, fd);
            if (old_fd != fd) 
            {
                IO::close(old_fd);
            }
        }
    }
    
    if (cache_) 
    {
        int cc = IO::lseek(fd, 0, SEEK_SET);
        ASSERT(cc == 0);
    }
    
    int cc;
    do {
        buf->reserve(buf->len() + 4096);
        cc = IO::read(fd, buf->end(), 4096, 0);
        ASSERTF(cc >= 0, "read failed %s", strerror(errno));
        buf->set_len(buf->len() + cc);
    } while (cc > 0);

    if (cache_) 
    {
        cache_->unpin(file_path);
    }
    else
    {
        IO::close(fd);
    }

    return 0;
}

//----------------------------------------------------------------------------
FileSystemIterator::FileSystemIterator(const std::string& path)
    : ent_(0)
{
    dir_ = opendir(path.c_str());
    ASSERT(dir_ != 0);
}

//----------------------------------------------------------------------------
FileSystemIterator::~FileSystemIterator()
{
    closedir(dir_);
}
    
//----------------------------------------------------------------------------
int 
FileSystemIterator::next()
{
  skip_dots:
    ent_ = readdir(dir_);

    if (ent_ != 0 && (strcmp(ent_->d_name, ".") == 0 ||
                      strcmp(ent_->d_name, "..") == 0))
    {
        goto skip_dots;
    }

    if (ent_ == 0) 
    {
        if (errno == EBADF) 
        {
            return DS_ERR;
        } 
        else 
        {
            return DS_NOTFOUND;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------
int 
FileSystemIterator::get_key(SerializableObject* key)
{
    ASSERT(ent_ != 0);
    
    KeyUnmarshal um(ent_->d_name, strlen(ent_->d_name), "-");
    int err = um.action(key);
    if (err != 0) {
        return DS_ERR;
    }

    return 0;
}

} // namespace oasys
