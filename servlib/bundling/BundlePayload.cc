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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/io/FileUtils.h>
#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/util/ScratchBuffer.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "BundleDaemon.h"
#include "BundleDaemonStorage.h"
#include "BundlePayload.h"
#include "storage/BundleStore.h"


#define FILEMODE_ALL (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)


namespace dtn {



oasys::SpinLock BundlePayload::dir_lock_;	///< coordinate attempts to create/remove directories



//----------------------------------------------------------------------
bool BundlePayload::test_no_remove_ = false;

//----------------------------------------------------------------------
BundlePayload::BundlePayload(oasys::SpinLock* lock)
    : Logger("BundlePayload", "/dtn/bundle/payload"),
      location_(DISK), length_(0), 
      cur_offset_(0), 
      base_offset_(0), 
      lock_(new oasys::SpinLock()),
      modified_(false)
{
    (void) lock;
}

//----------------------------------------------------------------------
void
BundlePayload::init(bundleid_t bundleid, location_t location)
{
    bundleid_ =  bundleid;

    if (location == BundlePayload::DEFAULT) {
        location_ = BundleDaemonStorage::params_.payload_location_;
    } else {
        location_ = location;
    }

    logpathf("/dtn/bundle/payload/%" PRIbid, bundleid);

    // nothing to do if there's no backing file
    if (location_ == MEMORY || location_ == NODATA) {
        return;
    }

    //dz debug
    oasys::ScopeLock l(lock_, "BundlePayload::init");

    // initialize the file handle for the backing store, but
    // immediately close it
    BundleStore* bs = BundleStore::instance();

    // XXX/demmer the simulator can't really deal with files, so this
    // is a hacky way to handle bundles that get created with a DISK
    // location in the simulator... a better fix would have this class
    // use oasys::FileBackedObjectStore and then have an in-memory
    // abstraction of the store that we use in the simulator, akin
    // to the memorydb version of the DurableStore
    if (bs->payload_dir() == "NO_PAYLOAD_FILES") {
        location_ = MEMORY;
        return;
    }


    //XXX/dz too many files in a directory causes major delay when creating files
    oasys::StringBuffer sb_dirpath("%s/%" PRIbid,
                             bs->payload_dir().c_str(), (bundleid / 10000));

    dir_path_ = sb_dirpath.c_str();


    // create the path into the subdirectory
    oasys::StringBuffer path("%s/bundle_%" PRIbid ".dat",
                             dir_path_.c_str(), bundleid);

    file_.logpathf("%s/file", logpath_);

    //XXX/dz Deleting the bundle payload now tries to delete the subdirectory as well
    //       which can happen between the mkdir and the file.open below. The workaround
    //       is to try multiple times (testing only ever required one retry)
    //       dz/ added the dir_lock_ to coordinate after syslog reported a nonfatal oops
    int err = 0;
    int open_errno = 0;
    int ctr = 0;
    while (++ctr < 4) {
        //create the subdirectory if it does not exist
        dir_lock_.lock(__func__);
        mkdir(dir_path_.c_str(), 0777 ); 

        err = 0;
        open_errno = 0;
        err = file_.open(path.c_str(), O_EXCL | O_CREAT | O_RDWR,
                             FILEMODE_ALL, &open_errno);
        dir_lock_.unlock();
    
        if (err < 0 && open_errno == EEXIST)
        {
            log_err("payload file %s already exists: overwriting and retrying",
                    path.c_str());

            err = file_.open(path.c_str(), O_RDWR, FILEMODE_ALL);
        }
        
        if (err < 0)
        {
            log_warn("error creating payload file attempt #%d - %s: %s",
                     ctr, path.c_str(), strerror(errno));
        }
        else
        {
            break;
        }
    }

    if (err < 0)
    {
        log_crit("aborting after error creating payload file %s: %s",
                 path.c_str(), strerror(errno));
        return;
    }

    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }

    if (length_ > 0) {
        // XXX/dz this will probably never be invoked
        sync_payload();
    }

    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::sync_payload()
{
    if (location_ == MEMORY || location_ == NODATA) {
        return;
    }

    oasys::ScopeLock scoplok(lock_, "BundlePayload::sync_payload");
    if (modified_) {

        pin_file();

        // flag to prevent releasing the database file to an app while a sync
        // is in progress - all others can read during this phase

        syncing_file_ = true;

        scoplok.unlock();

        fsync(file_.fd());
        unpin_file();

        // release the syncing flag after the unpin
        syncing_file_ = false;
        modified_ = false;
    }
}

//----------------------------------------------------------------------
void
BundlePayload::init_from_store(bundleid_t bundleid)
{
    location_ = DISK;


    oasys::ScopeLock l(lock_, "BundlePayload::init_from_store");

    BundleStore* bs = BundleStore::instance();


    //XXX/dz too many files in a directory causes major delay when creating files
    oasys::StringBuffer sb_dirpath("%s/%" PRIbid,
                             bs->payload_dir().c_str(), (bundleid / 10000));

    dir_path_ = sb_dirpath.c_str();


    // create the path into the subdirectory
    oasys::StringBuffer path("%s/bundle_%" PRIbid ".dat",
                             dir_path_.c_str(), bundleid);

    file_.logpathf("%s/file", logpath_);
    
    //create the subdirectory if it does not exist
    dir_lock_.lock(__func__);
    mkdir(dir_path_.c_str(), 0777 ); 
    dir_lock_.unlock();

    if (file_.open(path.c_str(), O_RDWR, FILEMODE_ALL) < 0)
    {
        // if the payload file open fails when trying to initialize,
        // we're in some amount of trouble, so set the location state
        // to NODATA to signal the upper layers that there's a problem
        // and that the bundle isn't valid
        log_crit("error opening payload file %s: %s",
                 path.c_str(), strerror(errno));
        location_ = NODATA;
        return;
    }

    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }
    unpin_file();
}

//----------------------------------------------------------------------
BundlePayload::~BundlePayload()
{
    delete_payload_file();

    delete lock_;
}

//----------------------------------------------------------------------
void
BundlePayload::delete_payload_file()
{
    if (BundleDaemon::final_cleanup()) {
        // Shutting down and releasing bundles from the lists which
        // can falsely trigger an attempt to delete the payload file
        // on some systems
        static bool bp_dpf_msg_output = false;

        if (!bp_dpf_msg_output) {
            bp_dpf_msg_output = true;

            log_notice("BundlePayload.%s: Detected attempt to delete payload file(s)"
                       " while shutting down and prevented it", 
                       __func__);
        }
        return;
    }


    if (location_ == DISK && file_.is_open()) {
        BundleStore::instance()->payload_fdcache()->close(file_.path());
        file_.set_fd(-1); // avoid duplicate close
        
        if (!test_no_remove_)
        {
            file_.unlink();

            dir_lock_.lock(__func__);
            rmdir(dir_path_.c_str());
            dir_lock_.unlock();
        }
    }
}

//----------------------------------------------------------------------
bool
BundlePayload::release_file(std::string& filename)
{
    bool result = false;

    oasys::ScopeLock scoplok(lock_, __func__);

    // prevent the BDStorage from trying to sync to disk if possible
    modified_ = false;

    // if sync is in progress then must wait for it to finish
    while (syncing_file_) {
        usleep(100000);
    }

    if (location_ == DISK) {
        // pin the file to reopen it incase t was evicted and closed
        if (pin_file()) {
            // moving the file up one level to the main bundle storage path and renaming it
            BundleStore* bs = BundleStore::instance();

            if (bs->payload_fdcache()->try_close_while_pinned(file_.path())) {
                file_.set_fd(-1); // prevent 2nd close

                oasys::StringBuffer new_filepath("%s/released_bundle_%" PRIbid ".dat",
                                                 bs->payload_dir().c_str(), bundleid_);
        
                int err = rename(file_.path(), new_filepath.c_str());

                if (err != 0) {
                    log_err("Error (%s) releasing and renaming bundle payload file from %s to %s",
                            strerror(errno), file_.path(), new_filepath.c_str());
                } else {
                    result = true;
                    filename = new_filepath.c_str();
                }
            } else {
                unpin_file();
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
BundlePayload::serialize(oasys::SerializeAction* a)
{
    u_int64_t u64_len = length_;
    u_int64_t u64_offset = base_offset_;

    a->process("length",      &u64_len);
    a->process("base_offset", &u64_offset);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        length_ = u64_len;
        base_offset_ = u64_offset;
    }
    
}

//----------------------------------------------------------------------
void
BundlePayload::set_length(size_t length)
{
    oasys::ScopeLock l(lock_, "BundlePayload::set_length");
    length_ = length;
    if (location_ == MEMORY) {
        data_.reserve(length);
        data_.set_len(length);
    }
}


//----------------------------------------------------------------------
bool
BundlePayload::pin_file() const
{
    if (location_ != DISK) {
        return true;
    }
    
    BundleStore* bs = BundleStore::instance();
    int fd = bs->payload_fdcache()->get_and_pin(file_.path());
    
    if (fd == -1) {
        int log_ctr = 49;
        while (file_.reopen(O_RDWR) < 0) {
            if ((errno == EMFILE) || (errno == ENFILE)) {
                // Too many open files - log about once every 5 seconds while problem persists
                if (++log_ctr >= 50) {
                    log_err("error reopening payload file but delaying and retrying - %s: %s",
                            file_.path(), strerror(errno));

                    if (!bs->payload_fdcache()->try_to_evict_one_file()) {
                        log_err("unable to close any file from the payload fd cache - all are in use");
                    }

                    log_ctr = 0;
                }

                usleep(100000);
                bs->payload_fdcache()->try_to_evict_one_file();
                errno = 0;
            } else {
                //XXX/dz removed check for file_path corruption since it has never been seen
                log_err("error reopening file %s: %s",
                        file_.path(), strerror(errno));

                return false;
            }
        }
    
        cur_offset_ = 0;

        int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
        if (fd != file_.fd()) {
            PANIC("duplicate entry in open fd cache");
        }
    } else {
        ASSERT(fd == file_.fd());
    }

    return true;
}

//----------------------------------------------------------------------
void
BundlePayload::unpin_file() const
{
    if (location_ != DISK) {
        return;
    }
    
    BundleStore::instance()->payload_fdcache()->unpin(file_.path());
}

//----------------------------------------------------------------------
void
BundlePayload::truncate(size_t length)
{
    oasys::ScopeLock l(lock_, "BundlePayload::truncate");
    
    ASSERT(length <= length_);
    length_     = length;
    cur_offset_ = length; // XXX/demmer is this right?
    
    switch (location_) {
    case MEMORY:
        data_.set_len(length);
        break;
    case DISK:
        pin_file();
        file_.truncate(length);
        unpin_file();
        modified_ = true;
        break;
    case NODATA:
    case DEFAULT:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
BundlePayload::copy_file(oasys::FileIOClient* dst) const
{

    //dz debug
    oasys::ScopeLock l(lock_, "BundlePayload::copy_file");

    //dzdebug ASSERT(location_ == DISK);
    if (location_ == DISK) {
        pin_file();
        file_.lseek(0, SEEK_SET);
        file_.copy_contents(dst, length());
        unpin_file();
    } else if (location_ == MEMORY) {
        // copying from memory to file
        ssize_t meg10 = 10 * 1024 * 10124;
        ssize_t buflen;
        ssize_t bytes_left_to_copy = length_;
        const char* cur_offset = (const char*) data_.buf();

        // write up to 10MB per write
        ssize_t cc;
        while (bytes_left_to_copy > 0) {
            buflen = meg10 < bytes_left_to_copy ? meg10 : bytes_left_to_copy;

            cc = dst->write(cur_offset, buflen);

            if (cc != buflen) {
                log_crit("error writing %zd bytes of payload from memory to file - result: %zd", buflen, cc);
                break;
            }

            cur_offset += buflen;
            bytes_left_to_copy -= buflen;
        }
    }
}

//----------------------------------------------------------------------
bool
BundlePayload::replace_with_file(const char* path)
{
    oasys::ScopeLock l(lock_, "BundlePayload::replace_with_file");
    
    ASSERT(location_ == DISK);
    std::string payload_path = file_.path();

    // first flush the old fd from the cache and unlink the file
    BundleStore* bs = BundleStore::instance();
    bs->payload_fdcache()->close(file_.path());
    file_.unlink();

    // now try to make the hard link
    int err = ::link(path, payload_path.c_str());
    if (err == 0) {
        log_debug("replace_with_file: successfully created link to %s", path);

        // unlink() clobbered path_ in file_, so we have to set it
        // again and re-open the copy
        file_.set_path(payload_path);
        if (file_.reopen(O_RDWR) < 0) {
            log_err("replace_with_file: error reopening file: %s",
                    strerror(errno));
            return false;
        }
        
    } else {
        // ::link failed
        err = errno;
        if (err != EXDEV) {
            log_err("error linking to path '%s': %s", path, strerror(err));
            return false;
        }
        
        // copy the contents if they're on different filesystems
        log_debug("replace_with_file: link failed: %s", strerror(err));
        
        oasys::FileIOClient src;
        int fd = src.open(path, O_RDONLY, &err);
        if (fd < 0) {
            log_err("error opening path '%s' for reading: %s",
                    path, strerror(err));
            return false;
        }
        
        file_.set_path(payload_path);
        if (file_.reopen(O_RDWR | O_CREAT, FILEMODE_ALL) < 0) {
            log_err("replace_with_file: error reopening file: %s",
                    strerror(err));
            return false;
        }
        
        src.copy_contents(&file_);
        src.close();
    }

    set_length(oasys::FileUtils::size(file_.path()));

    // now need to re-add the entry to the cache
    ASSERT(file_.fd() != -1);
    int fd = bs->payload_fdcache()->put_and_pin(file_.path(), file_.fd());
    if (fd != file_.fd()) {
        PANIC("duplicate entry in open fd cache");
    }
    unpin_file();

    modified_ = true;

    return true;
}
    
//----------------------------------------------------------------------
void
BundlePayload::internal_write(const u_char* bp, size_t offset, size_t len)
{
    // the caller should have pinned the fd
    if (location_ == DISK) {
        ASSERT(file_.is_open());
    }
    ASSERT(lock_->is_locked_by_me());
    ASSERT(length_ >= (offset + len));

    switch (location_) {
    case MEMORY:
        memcpy(data_.buf() + offset, bp, len);
        break;
    case DISK:
        // check if we need to seek
        if (cur_offset_ != offset) {
            file_.lseek(offset, SEEK_SET);
            cur_offset_ = offset;
        }
        int cc;
        
        cc = file_.writeall((char*)bp, len);
        if (cc < 0) {
            log_err("%s: error writing %zu bytes at offset %zu to file(fd: %d): %s - %s",
                    __func__, len, offset, file_.fd(), file_.path(), strerror(errno));
        } else if ((size_t) cc < len) {
            cur_offset_ += cc;
            log_err("%s: error writing %zu bytes at offset %zu (only wrote %d) to file(fd: %d): %s - %s",
                    __func__, len, offset, cc, file_.fd(), file_.path(), strerror(errno));
        } else {
            cur_offset_ += len;
        }

        modified_ = true;
        break;
    case NODATA:
    case DEFAULT:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
BundlePayload::set_data(const u_char* bp, size_t len)
{

    //dz debug
    oasys::ScopeLock l(lock_, "BundlePayload::set_data");

    set_length(len);
    write_data(bp, 0, len);
}

//----------------------------------------------------------------------
void
BundlePayload::set_data(const std::string& data)
{
    set_data((const u_char*)(data.data()), data.length());
}

//----------------------------------------------------------------------
void
BundlePayload::append_data(const u_char* bp, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::append_data");

    size_t old_length = length_;
    set_length(length_ + len);
    
    pin_file();
    internal_write(bp, old_length, len);
    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::write_data(const u_char* bp, size_t offset, size_t len)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");
    
    ASSERT(length_ >= (len + offset));
    pin_file();
    internal_write(bp, offset, len);
    unpin_file();
}

//----------------------------------------------------------------------
void
BundlePayload::write_data(const BundlePayload& src, size_t src_offset,
                          size_t len, size_t dst_offset)
{
    oasys::ScopeLock l(lock_, "BundlePayload::write_data");

    log_debug("write_data: file=%s length_=%zu src_offset=%zu "
              "dst_offset=%zu len %zu",
              file_.path(),
              length_, src_offset, dst_offset, len);

    ASSERT(length_       >= dst_offset + len);
    ASSERT(src.length() >= src_offset + len);

    // XXX/mho: todo - for cases where we're creating a fragment from
    // an existing bundle, make a hard link for the new fragment and
    // store the offset in base_offset_

    // XXX/demmer todo -- we should copy the payload in max-length chunks
    
    oasys::ScratchBuffer<u_char*, 1024> buf(len);
    const u_char* bp = src.read_data(src_offset, len, buf.buf());

    pin_file();
    internal_write(bp, dst_offset, len);
    unpin_file();
}

//----------------------------------------------------------------------
const u_char*
BundlePayload::read_data(size_t offset, size_t len, u_char* buf)
{
    oasys::ScopeLock l(lock_, "BundlePayload::read_data");
    
    ASSERTF(length_ >= (offset + len),
            "length=%zu offset=%zu len=%zu",
            length_, offset, len);

    ASSERT(buf != NULL);
    
    switch(location_) {
    case MEMORY:
        memcpy(buf, data_.buf() + offset, len);
        break;

    case DISK:
        pin_file();
        
        // check if we need to seek first
        if (offset != cur_offset_) {
            file_.lseek(offset, SEEK_SET);
        }
        
        file_.readall((char*)buf, len);
        cur_offset_ = offset + len;
        
        unpin_file();
        break;

    case NODATA:
    case DEFAULT:
        NOTREACHED;
    }

    return buf;
}


} // namespace dtn
