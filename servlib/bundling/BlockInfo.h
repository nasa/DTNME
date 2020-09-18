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

#ifndef _BUNDLEBLOCKINFO_H_
#define _BUNDLEBLOCKINFO_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/serialize/SerializableVector.h>
#include <oasys/util/ScratchBuffer.h>

#include "security/BSPProtectionInfo.h"

#include "BP_Local.h"
#include "Dictionary.h"
#include "contacts/Link.h"
#include <string>

namespace dtn {

class BlockProcessor;
class BP_Local;

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
    BlockInfo(BlockProcessor* owner, const BlockInfo* source = NULL);

    /// Constructor for unserializing
    BlockInfo(oasys::Builder& builder);

    /// Copy constructor to increment refcount for locals_
    BlockInfo(const BlockInfo& bi);

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
    BlockProcessor*   owner()          const { return owner_; }
    const BlockInfo*  source()         const { return source_; }
    const EndpointIDVector& eid_list() const { return eid_list_; }
    const DataBuffer& contents()       const { return contents_; }
    BP_Local*         locals()         const { return locals_.object(); }
    u_int32_t         data_length()    const { return data_length_; }
    u_int32_t         data_offset()    const { return data_offset_; }
    u_int32_t         full_length()    const { return (data_offset_ +
                                                       data_length_); }
    u_char*           data()           const { return (contents_.buf() +
                                                       data_offset_); }
    bool              complete()       const { return complete_; }
    bool              reloaded()       const { return reloaded_; }
    bool              last_block()     const;
    ///@}

    /// @{ Mutating accessors
    void        set_owner(BlockProcessor* o) { owner_ = o; }
    void        set_eid_list(const EndpointIDVector& l) { eid_list_ = l; }
    void        set_complete(bool t)         { complete_ = t; }
    void        set_data_length(u_int32_t l) { data_length_ = l; }
    void        set_data_offset(u_int32_t o) { data_offset_ = o; }
    DataBuffer* writable_contents()          { return &contents_; }
    void        set_locals(BP_Local* l);
    void        add_eid(EndpointID e)        { eid_list_.push_back(e); }
    void        set_reloaded(bool t)         { reloaded_ = t; }
    /// @}

    /// @{ These accessors need special case processing since the
    /// primary block doesn't have the fields in the same place.
    int       type()  const;
    u_int64_t flags() const;
    void      set_flag(u_int64_t flag);
    /// @}

    /// Virtual from SerializableObject
    virtual void serialize(oasys::SerializeAction* action);

#ifdef BSP_ENABLED
    oasys::SerializableVector<BSPProtectionInfo> bsp;
    uint8_t original_block_type;
#endif
protected:
    BlockProcessor*  owner_;       ///< Owner of this block
    u_int16_t        owner_type_;  ///< Extracted from owner
    const BlockInfo* source_;      ///< Owner of this block
    EndpointIDVector eid_list_;    ///< List of EIDs used in this block
    DataBuffer       contents_;    ///< Block contents with length set to
                                   ///  the amount currently in the buffer
    BP_LocalRef      locals_;      ///< Local variable storage for block processor
    u_int32_t        data_length_; ///< Length of the block data (w/o preamble)
    u_int32_t        data_offset_; ///< Offset of first byte of the block data
    bool             complete_;    ///< Whether or not this block is complete
    bool             reloaded_;    ///< Whether or not this block is reloaded
                                   ///  from store (set in ::reload_post_process
                                   ///  of the BlockProcessor classes)
};

/**
 * Class for a vector of BlockInfo structures.
 */
class BlockInfoVec : public oasys::SerializableVector<BlockInfo> {
public:
    /**
     * Default constructor.
     */
    BlockInfoVec();
    
    /**
     * Append a block using the given processor and optional source
     * block.
     *
     * @return the newly allocated block -- note that this pointer may
     * be invalidated with a future call that changes the vector.
     */
    BlockInfo* append_block(BlockProcessor*  owner,
                            const BlockInfo* source = NULL);

    /**
     * Find the block for the given type.
     *
     * @return the block or NULL if not found
     */
    const BlockInfo* find_block(u_int8_t type) const;
    
    /**
     * Check if an entry exists in the vector for the given block type.
     */
    bool has_block(u_int8_t type) const { return find_block(type) != NULL; }

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
    BlockInfoVec* create_blocks(const LinkRef& link);
    
    /**
     * Find the BlockInfoVec for the given link.
     *
     * @return Pointer to the BlockInfoVec or NULL if not found
     */
    BlockInfoVec* find_blocks(const LinkRef& link) const;
    
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
        
        BlockInfoVec* blocks_;
        LinkRef       link_;
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
