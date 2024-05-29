#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "FileBackedObject.h"

#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../debug/DebugUtils.h"
#include "../io/FileIOClient.h"
#include "../io/FileUtils.h"

#include "../serialize/Serialize.h"
#include "../serialize/StreamSerialize.h"
#include "../storage/FileBackedObjectStream.h"

namespace oasys {

//----------------------------------------------------------------------------
FileBackedObject::Tx::Tx(FileBackedObject* backing_file, int flags)
    : Logger("FileBackedObject", "/store/file-backed/tx"),
      original_file_(backing_file),
      tx_file_(0)
{
    logpathf("/store/file-backed/tx/%s", original_file_->filename().c_str());

    std::string tx_filename = original_file_->filename() + ".tx";

    if (flags & FileBackedObject::INIT_BLANK)
    {
        int fd = ::open(tx_filename.c_str(), 
                        O_WRONLY | O_CREAT | O_EXCL,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        ::close(fd);
    }
    else
    {
        int err = FileUtils::fast_copy(original_file_->filename().c_str(), 
                                       tx_filename.c_str());
        ASSERT(err == 0);
    }
    tx_file_ = new FileBackedObject(tx_filename, flags);

    log_debug("tx started");
}
        
//----------------------------------------------------------------------------
FileBackedObject::Tx::~Tx()
{
    commit();
}
        
//----------------------------------------------------------------------------
FileBackedObject*
FileBackedObject::Tx::object()
{
    return tx_file_;
}
        
//----------------------------------------------------------------------------
void
FileBackedObject::Tx::commit()
{
    if (tx_file_ == 0)
    {
        return;
    }

    tx_file_->fsync_data();
    int err = rename(tx_file_->filename().c_str(), 
                     original_file_->filename().c_str());
    ASSERT(err == 0);
    original_file_->reload();
    delete_z(tx_file_);

    log_debug("tx committed");    
}

//----------------------------------------------------------------------------
void
FileBackedObject::Tx::abort()
{
    tx_file_->unlink();
    delete_z(tx_file_);
    
    log_debug("tx aborted");
}

//----------------------------------------------------------------------------
FileBackedObject::OpenScope::OpenScope(FileBackedObject* obj)
    : obj_(obj)
{
    oasys::ScopeLock l(&obj_->lock_, "FileBackedObject::OpenScope()");
    ++obj_->open_count_;

    obj_->open();
}

//----------------------------------------------------------------------------
FileBackedObject::OpenScope::~OpenScope()
{
    oasys::ScopeLock l(&obj_->lock_, "FileBackedObject::OpenScope()");
    --obj_->open_count_;

    if (obj_->open_count_ == 0)
    {
        obj_->close();
    }
}

//----------------------------------------------------------------------------
FileBackedObject::FileBackedObject(const std::string& filename, 
                                   int flags)
    : filename_(filename),
      fd_(-1),
      flags_(flags),
      lock_("/st/filebacked/lock"),
      open_count_(0)
{
}

//----------------------------------------------------------------------------
FileBackedObject::~FileBackedObject()
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::~Destructor");

    if (fd_ != -1)
    {
        ::close(fd_);
        log_debug_p("/st/filebacked", "destruct %p fd = -1", this);
        fd_ = -1;
    }
}

//----------------------------------------------------------------------------
FileBackedObject::TxHandle 
FileBackedObject::start_tx(int flags)
{
    return TxHandle(new Tx(this, flags));
}
    
//----------------------------------------------------------------------------
void 
FileBackedObject::set_flags(int flags)
{
    flags_ = flags;
    close();
}

//----------------------------------------------------------------------------
void 
FileBackedObject::get_stats(struct stat* stat_buf) const
{
    int err = stat(filename_.c_str(), stat_buf);

    FileUtils::StatFormat statfmt(*stat_buf);
    log_debug_p("/store/file-backed", "stat: *%p", &statfmt);

    ASSERT(err == 0);
}

//----------------------------------------------------------------------------
void 
FileBackedObject::set_stats(struct stat* stat_buf)
{
    (void) stat_buf;
    // XXX/bowei -- LALALA
    NOTIMPLEMENTED;
}

//----------------------------------------------------------------------------
size_t
FileBackedObject::size() const
{
    struct stat stat_buf;
    get_stats(&stat_buf);
    
    return stat_buf.st_size;
}

//----------------------------------------------------------------------------
size_t 
FileBackedObject::read_bytes(size_t offset, u_char* buf, size_t length) const
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::read_bytes");

    open();

    ASSERT(fd_ != -1);
    
    if (cur_offset_ != offset)
    {
        off_t off = lseek(fd_, offset, SEEK_SET);

        if (off == -1 && size() == 0)
        {
            off = 0;
        }
        ASSERT(static_cast<size_t>(off) == offset);

        cur_offset_ = offset;
    }

    int cc = read(fd_, buf, length);
    cur_offset_ += cc;
    
    close();

    return cc;
}

//----------------------------------------------------------------------------
size_t 
FileBackedObject::write_bytes(size_t offset, const u_char* buf, size_t length)
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::write_bytes");

    open();

    ASSERT(fd_ != -1);

    if (cur_offset_ != offset)
    {
        off_t off = lseek(fd_, offset, SEEK_SET);

        if (off == -1 && size() == 0)
        {
            off = 0;
        }
        ASSERT(static_cast<size_t>(off) == offset);
        cur_offset_ = offset;
    }
    
    int cc = write(fd_, buf, length);
    ASSERT(static_cast<size_t>(cc) == length);
    cur_offset_ += cc;

    close();
    
    return cc;
}

//----------------------------------------------------------------------------
size_t 
FileBackedObject::append_bytes(const u_char* buf, size_t length)
{
    open();

    off_t off = lseek(fd_, 0, SEEK_END);
    if (off == -1 && size() == 0)
    {
        off = 0;
    }
    cur_offset_ = static_cast<size_t>(off);

    return write_bytes(cur_offset_, buf, length);
}

//----------------------------------------------------------------------
bool
FileBackedObject::replace_with_file(const std::string& filename)
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::replace_with_file");

    std::string old_filename = filename_;
    unlink();
    ASSERT(fd_ == -1); // unlink also closes

    int err = ::link(filename.c_str(), old_filename.c_str());
    if (err == 0) {
        // unlink() clobbered the filename and the flags
        filename_ = old_filename;
        flags_   &= ~UNLINKED;
     
        log_debug_p("/st/filebacked",
                    "replace_with_file: successfully created link from %s -> %s",
                  old_filename.c_str(), filename.c_str());
        return true;
    }

    // XXX/demmer this would be much simpler if FileBackedObject used
    // a FileIOClient underneath...
    err = errno;
    if (err == EXDEV) {
        log_debug_p("/st/filebacked",
                    "replace_with_file: link failed: %s", strerror(err));
        
        oasys::FileIOClient src;
        int fd = src.open(filename.c_str(), O_RDONLY, &err);
        if (fd < 0) {
            log_err_p("/st/filebacked",
                      "error opening file '%s' for reading: %s",
                      filename.c_str(), strerror(err));
            return false;
        }
        
        oasys::FileIOClient dst;
        int fd2 = dst.open(old_filename.c_str(),
                           O_WRONLY | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
                           &err);
        if (fd2 < 0) {
            log_err_p("/st/filebacked",
                      "error opening file '%s' for reading: %s",
                      old_filename.c_str(), strerror(err));
            return false;
        }
        
        src.copy_contents(&dst);
        src.close();
        dst.close();
        
        // unlink() clobbered the filename and the flags
        filename_ = old_filename;
        flags_   &= ~UNLINKED;

        log_debug_p("/st/filebacked",
                    "replace_with_file: successfully copied %s -> %s",
                    old_filename.c_str(), filename.c_str());
        return true;
    }

    log_err_p("/st/filebacked",
              "error linking to path '%s': %s",
              filename.c_str(), strerror(err));

    // XXX/demmer the state of the file is messed up at this point
    return false;
}
    

//----------------------------------------------------------------------------
void 
FileBackedObject::truncate(size_t size)
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::truncate");

    open();

    ASSERT(fd_ != -1);
    
    int err = ftruncate(fd_, size);
    ASSERT(err == 0);

    close();
}

//----------------------------------------------------------------------------
void 
FileBackedObject::fsync_data()
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::fsync_data");
#ifdef HAVE_FDATASYNC
    fdatasync(fd_);
#else
    fsync(fd_);
#endif
}

//----------------------------------------------------------------------------
int
FileBackedObject::serialize(const SerializableObject* obj, int offset)
{
    ScopeLock l(&lock_, "FileBackedObject::serialize");
    OpenScope o(this);

    open();
    size_t abs_offset = size() + offset;
    
    if (abs_offset != cur_offset_)
    {
        off_t off = lseek(fd_, static_cast<off_t>(abs_offset), SEEK_SET);
        if (off == -1 && size() == 0)
        {
            off = 0;
        }
        cur_offset_ = static_cast<size_t>(off);
    }    
    
    FileBackedObjectOutStream stream(this, cur_offset_);
    StreamSerialize serial(&stream, Serialize::CONTEXT_LOCAL);
    int ret = serial.action(obj);
    
    return ret;
}

//----------------------------------------------------------------------------
int
FileBackedObject::unserialize(SerializableObject* obj)
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::unserialize");

    FileBackedObjectInStream stream(this);
    StreamUnserialize serial(&stream, Serialize::CONTEXT_LOCAL);

    return serial.action(obj);
}

//----------------------------------------------------------------------------
void
FileBackedObject::open() const
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::open");

    ASSERT(! (flags_ & UNLINKED));

    if (fd_ != -1)
    {
        return;
    }
    
    fd_ = ::open(filename_.c_str(), O_RDWR);
    ASSERT(fd_ != -1);

    cur_offset_ = 0;
}

//----------------------------------------------------------------------------
void
FileBackedObject::close() const
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::close");

    if (fd_ == -1 || open_count_ > 0)
    {
        return;
    }

    ::close(fd_);

    log_debug_p("/st/filebacked", "close %p fd = -1", this);
    fd_ = -1;
}

//----------------------------------------------------------------------------
void
FileBackedObject::unlink()
{
    // XXX/demmer seems better for unlink to just unlink and not close
    // so we can use these for temporary files
    oasys::ScopeLock l(&lock_, "FileBackedObject::unlink");

    if (fd_ != 0)
    {
        ::close(fd_);
        log_debug_p("/st/filebacked", "unlink %p fd = -1", this);
        fd_ = -1;
    }
    
    int err = ::unlink(filename_.c_str());
    ASSERT(err == 0);
    
    filename_ = "/INVALID_FILE";
    flags_ |= UNLINKED;
}

//----------------------------------------------------------------------------
void 
FileBackedObject::reload()
{
    oasys::ScopeLock l(&lock_, "FileBackedObject::reload");

    close();
    open();
}

} // namespace oasys
