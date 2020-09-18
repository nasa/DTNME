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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include <oasys/serialize/Serialize.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/io/FileIOClient.h>

namespace dtn {

class BundleStore;

/**
 * The representation of a bundle payload.
 *
 * This is abstracted into a separate class to allow the daemon to
 * separately manage the serialization of header information from the
 * payload.
 *
 * Note that this implementation doesn't support payloads larger than
 * 4GB. XXX/demmer fix this.
 *
 */
class BundlePayload : public oasys::SerializableObject, public oasys::Logger {
public:
    BundlePayload(oasys::SpinLock* lock);
    virtual ~BundlePayload();
    
    /**
     * Options for payload location state.
     */
    typedef enum {
        MEMORY = 1,	/// in memory only (TempBundle)
        DISK   = 2,	/// on disk
        NODATA = 3,	/// no data storage at all (used for simulator)
    } location_t;
    
    /**
     * Actual payload initialization function.
     */
    void init(bundleid_t bundleid, location_t location = DISK);
  
    /**
     * Initialization when re-reading the database
     */
    void init_from_store(bundleid_t bundleid);
  
    /**
     * Sync the payload file to disk (if location = DISK)
     */
    void sync_payload();

    /**
     * Set the payload length in preparation for filling in with data.
     */
    void set_length(size_t len);

    /**
     * Truncate the payload. Used for reactive fragmentation.
     */
    void truncate(size_t len);
    
    /**
     * The payload length.
     */
    size_t length() const { return length_; }

    /**
     * The payload location.
     */
    location_t location() const { return location_; }
    
    /**
     * Set the payload data and length.
     */
    void set_data(const u_char* bp, size_t len);

    /**
     * Set the payload data and length.
     */
    void set_data(const std::string& data);

    /**
     * Append a chunk of payload data, extending the length to
     * accomodate the new data.
     */
    void append_data(const u_char* bp, size_t len);

    /**
     * Write a chunk of payload data at the specified offset. The
     * length must have been previously set to at least offset + len.
     */
    void write_data(const u_char* bp, size_t offset, size_t len);

    /**
     * Writes len bytes of payload data from from another payload at
     * the given src_offset to the given dst_offset.
     */
    void write_data(const BundlePayload& src, size_t src_offset,
                    size_t len, size_t dst_offset);

    /**
     * Copy (or link) the payload to the given file client object
     */
    void copy_file(oasys::FileIOClient* dst) const;

    /**
     * Replace the underlying file with a hard link to the given path
     * or a copy of the file contents if the link can't be created.
     */
    bool replace_with_file(const char* path);

    /**
     * Return the filename.
     */
    const std::string& filename() const
    {
        ASSERT(location_ == DISK);
        return file_.path_str();
    }
    
    /**
     * Get a pointer to the in-memory scratch buffer.
     */
    oasys::ScratchBuffer<u_char*>* memory_buf()
    {
        ASSERT(location_ == MEMORY);
        return &data_;
    }

    /**
     * Get a pointer to the in-memory scratch buffer (const variant
     */
    const oasys::ScratchBuffer<u_char*>* memory_buf() const
    {
        ASSERT(location_ == MEMORY);
        return &data_;
    }

    /**
     * Copy out a chunk of payload data into the supplied buffer.
     * @return pointer to the buffer for convenience
     */
    const u_char* read_data(size_t offset, size_t len, u_char* buf);

    /**
     * Since read_data doesn't really change anything of substance in
     * the payload class (just the internal bookkeeping fields), we
     * define a "const" variant that just casts itself away and calls
     * the normal variant.
     */
    const u_char* read_data(size_t offset, size_t len, u_char* buf) const
    {
        return const_cast<BundlePayload*>(this)->
            read_data(offset, len, buf);
    }
     
    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);

    static bool test_no_remove_;    ///< test: don't rm payload files

protected:
    bool pin_file() const;
    void unpin_file() const;
    void internal_write(const u_char* bp, size_t offset, size_t len);

    location_t location_;	///< location of the data 
    oasys::ScratchBuffer<u_char*> data_; ///< payload data if in memory
    size_t length_;     	///< the payload length
    mutable oasys::FileIOClient file_;	///< file handle
    mutable size_t cur_offset_;	///< cache of current fd position
    size_t base_offset_;	///< for fragments, offset into the file (todo)
    oasys::SpinLock* lock_;	///< the lock for the given bundle
    bool modified_;             ///< whether or not a fsync is needed

    std::string dir_path_;      ///< directory path of the payload file

    bundleid_t bundleid_;
};

} // namespace dtn

#endif /* _BUNDLE_PAYLOAD_H_ */
