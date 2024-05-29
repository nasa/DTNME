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

#ifndef _BUNDLEBLOCKINFO_H_
#define _BUNDLEBLOCKINFO_H_

#include <memory>

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/serialize/SerializableSPtrVector.h>
#include <third_party/oasys/util/ScratchBuffer.h>

#include "BP_Local.h"
#include "Dictionary.h"
#include "contacts/Link.h"
#include <string>

namespace dtn {

class BlockProcessor;
class BlockInfo;
class BlockInfoVec;
class BP_Local;

typedef std::shared_ptr<BlockInfo> SPtr_BlockInfo;
typedef std::shared_ptr<BlockInfoVec> SPtr_BlockInfoVec;




/**
 * Class used to store unparsed bundle blocks and associated metadata
 * about them.
 */
class BlockInfo : public oasys::SerializableObject {
public:
    /// To store the formatted block data, we use a scratch buffer
    /// with 64 bytes of static buffer space which should be
    /// sufficient to cover most blocks and avoid mallocs.
    typedef oasys::ScratchBuffer<u_char*, 64> DataBuffer;
    
    /// Default constructor assigns the owner and optionally the
    /// BlockInfo source (i.e. the block as it arrived off the wire)
    BlockInfo(BlockProcessor* owner);

    /// Default constructor assigns the owner and optionally the
    /// BlockInfo source (i.e. the block as it arrived off the wire)
    BlockInfo(BlockProcessor* owner, const SPtr_BlockInfo source);

    /// Constructor for unserializing
    BlockInfo(oasys::Builder& builder);

    /// Copy constructor to increment refcount for locals_
    BlockInfo(const BlockInfo& bi);
    BlockInfo(const SPtr_BlockInfo& bi);

    /// copy operator
    BlockInfo& operator=(const BlockInfo& other);
    
    /// Move assignment for C++11
    BlockInfo& operator=(BlockInfo&& other) noexcept;
    
    /**
     * Virtual destructor.
     */
    virtual ~BlockInfo();

    /**
     * List owner indicator (not transmitted)
     */
    typedef enum {
        LIST_NONE           = 0x00,
        LIST_RECEIVED       = 0x01,
        LIST_API            = 0x02,
        LIST_EXT            = 0x03,
        LIST_XMIT           = 0x04
    } list_owner_t;
    
    /// @{ Accessors
    BlockProcessor*   owner()              const { return owner_; }
    const SPtr_BlockInfo  source()         const { return source_; }
    const EndpointIDVector& eid_list()     const { return eid_list_; }
    const DataBuffer& contents()           const { return contents_; }
    BP_Local*         locals()             const { return locals_.object(); }
    uint64_t          block_number()       const { return block_number_; }
    uint64_t          data_length()        const { return data_length_; }
    uint64_t          data_offset()        const { return data_offset_; }
    uint64_t          full_length()        const { return (data_offset_ +
                                                           data_length_ +
                                                           crc_length_); }
    u_char*           data()               const { return (contents_.buf() +
                                                           data_offset_); }
    bool              complete()           const { return complete_; }
    bool              reloaded()           const { return reloaded_; }
    bool              last_block()         const;

    // BP7
    uint64_t          crc_type()           const { return crc_type_; }
    size_t            crc_length()         const { return crc_length_; }
    size_t            crc_offset()         const { return crc_offset_; }
    const uint8_t*    crc_cbor_bytes()     const { return crc_cbor_bytes_; } // first byte is the CBOR header byte
    size_t            crc_bytes_received() const { return crc_bytes_received_; } // used by BP7_PayloadBlockProcessor
    bool              crc_checked()        const { return crc_checked_; }
    bool              crc_validated()      const { return crc_validated_; }
    size_t            bpv7_EOB_offset()    const { return bpv7_EOB_offset_; }
    ///@}

    /// @{ Mutating accessors
    void        set_owner(BlockProcessor* o) { owner_ = o; }
    void        set_eid_list(const EndpointIDVector& l) { eid_list_ = l; }
    void        set_complete(bool t)         { complete_ = t; }
    void        set_block_number(uint64_t n) { block_number_ = n; }
    void        set_data_length(uint64_t l) { data_length_ = l; }
    void        set_data_offset(uint64_t o) { data_offset_ = o; }
    DataBuffer* writable_contents()          { return &contents_; }
    void        set_locals(BP_Local* l);
    void        add_eid(SPtr_EID& sptr_e)        { eid_list_.push_back(sptr_e); }
    void        set_reloaded(bool t)         { reloaded_ = t; }

    // BP7 
    void        set_crc_type(uint64_t t)     { crc_type_ = t; }
    void        set_crc_length(size_t t)     { crc_length_ = t; }
    void        set_crc_offset(size_t t)     { crc_offset_ = t; }
    int         add_crc_cbor_bytes(uint8_t* buf, size_t buflen); // return -1 = error; else total crc bytes received so far by BP7_PaylaodBlockProcessor


    void        set_crc_checked(bool b)      { crc_checked_ = b; }
    void        set_crc_validated(bool b)    { crc_validated_ = b; }
    void        set_bpv7_EOB_offset(size_t t)     { bpv7_EOB_offset_ = t; }
    /// @}

    /// @{ These accessors need special case processing since the
    /// primary block doesn't have the fields in the same place.
    int       type()  const;
    u_int64_t block_flags() const;
    void      set_flag(u_int64_t flag);
    /// @}

    /// Virtual from SerializableObject
    virtual void serialize(oasys::SerializeAction* action);

protected:
    BlockProcessor*  owner_;              ///< Owner of this block
    u_int16_t        owner_type_;         ///< Extracted from owner
    SPtr_BlockInfo   source_;             ///< Owner of this block
    EndpointIDVector eid_list_;           ///< List of EIDs used in this block
    DataBuffer       contents_;           ///< Block contents with length set to
                                          ///  the amount currently in the buffer
    BP_LocalRef      locals_;             ///< Local variable storage for block processor
    uint64_t         block_number_;       ///< Bundle Protocol v7 Block Number
    uint64_t         block_flags_;        ///< Block processing flags
    size_t           data_length_;        ///< Length of the block data (w/o preamble)
    size_t           data_offset_;        ///< Offset of first byte of the block data
    bool             complete_;           ///< Whether or not this block is complete
    bool             reloaded_;           ///< Whether or not this block is reloaded
                                          ///  from store (set in ::reload_post_process
                                          ///  of the BlockProcessor classes)
    uint64_t         crc_type_;           ///< CRC Type for the block. 0=none, 1=CRC16, 2=CRC32c (BPv7)
    size_t           crc_length_;         ///< num CBOR bytes encoding the CRC (BPv7)
    size_t           crc_offset_;         ///< Offset of first byte of the CRC CBOR field (BPv7)
    size_t           crc_bytes_received_; ///<  Number of CRC CBOR bytes received (BPv7)
    uint8_t          crc_cbor_bytes_[6];  ///< storage for the CRC CBOR bytes (BPv7)
    bool             crc_checked_;        ///< whether yhe CRC for the block checked
    bool             crc_validated_;      ///< whether the checked CRC was validated or failed
    size_t           bpv7_EOB_offset_;    ///< Offset to closing CBOR break character
                                          ///  indicating End of Bundle [CBOR array] (BPv7)
                                          ///  - used by BP7_PayloadBLockProcessor since it
                                          ///    is always the last block of a bundle
};



/**
 * Class for a vector of BlockInfo structures.
 */
class BlockInfoVec : public oasys::SerializableSPtrVector<BlockInfo> {
public:
    /**
     * Default constructor.
     */
    BlockInfoVec();
    
    /**
     * Creates and appends a block using the given processor and optional source
     * block. 
     *
     * @return a shread_ptr to the newly allocated block
     */
    SPtr_BlockInfo append_block(BlockProcessor*  owner);
    SPtr_BlockInfo append_block(BlockProcessor*  owner, const SPtr_BlockInfo source);

    /**
     * Find the block for the given type.
     *
     * @return the block or nullptr if not found
     */
    SPtr_BlockInfo find_block(u_int8_t type) const;
    
    /**
     * Check if an entry exists in the vector for the given block type.
     */
    bool has_block(u_int8_t type) const { 
        SPtr_BlockInfo blkptr = find_block(type);
        return (blkptr != nullptr); 
    }

    /**
     * Return the dictionary.
     */
    Dictionary* dict()  { return &dict_; }

    /**
     * Error values stored by processing routines
     *   -- accessors
     */
    u_int32_t         error_major()     const { return error_major_; }
    u_int32_t         error_minor()     const { return error_minor_; }
    u_int32_t         error_debug()     const { return error_debug_; }

    /**
     * Error values stored by processing routines
     *   -- mutating accessors
     */
    void        set_error_major(u_int32_t e)  { error_major_ = e; }
    void        set_error_minor(u_int32_t e)  { error_minor_ = e; }
    void        set_error_debug(u_int32_t e)  { error_debug_ = e; }

protected:
    /**
     * Error values stored by processing routines
     */
    u_int32_t    error_major_;
    u_int32_t    error_minor_;
    u_int32_t    error_debug_;

    /**
     * Dictionary for this vector of BlockInfo structures
     */
    Dictionary   dict_;

    /**
     * Disable the copy constructor and assignment operator.
     */
    NO_ASSIGN_COPY(BlockInfoVec);
};




/**
 * A set of BlockInfoVecs, one for each outgoing link.
 */
class LinkBlockSet {
public:
    LinkBlockSet(oasys::Lock* lock) : lock_(lock) {}
    
    /**
     * Destructor that clears the set.
     */
    virtual ~LinkBlockSet();
    
    /**
     * Create a new BlockInfoVec for the given link.
     *
     * @return Pointer to the new BlockInfoVec
     */
    SPtr_BlockInfoVec create_blocks(const LinkRef& link);
    
    /**
     * Find the BlockInfoVec for the given link.
     *
     * @return Pointer to the BlockInfoVec or nullptr if not found
     */
    SPtr_BlockInfoVec find_blocks(const LinkRef& link) const;
    
    /**
     * Remove the BlockInfoVec for the given link.
     */
    void delete_blocks(const LinkRef& link);

protected:
    /**
     * Struct to hold a block list and a link pointer. Note that we
     * allow the STL vector to copy the pointers to both the block
     * list and the link pointer. This is safe because the lifetime of
     * the BlockInfoVec object is defined by create_blocks() and
     * delete_blocks().
     */
    struct Entry {
        Entry(const LinkRef& link);
        
        SPtr_BlockInfoVec blocks_;
        LinkRef           link_;
    };

    typedef std::vector<Entry> Vector;
    typedef std::vector<Entry>::iterator iterator;
    typedef std::vector<Entry>::const_iterator const_iterator;
    Vector entries_;
    oasys::Lock* lock_;

    /**
     * Disable the copy constructor and assignment operator.
     */
    NO_ASSIGN_COPY(LinkBlockSet);
};

} // namespace dtn

#endif /* _BUNDLEBLOCKINFO_H_ */
