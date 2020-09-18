/*
 *    Copyright 2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifndef _METADATA_BLOCK_H_
#define _METADATA_BLOCK_H_

#include <oasys/thread/Mutex.h>

#include "BlockInfo.h"
#include "BlockProcessor.h"

namespace dtn {

/**
 * The representation of a metadata extension block.
 */
class MetadataBlock : public BP_Local, public oasys::SerializableObject {
public:

    // Constructors
    MetadataBlock(): lock_("MetadataBlock"),
        id_(MetadataBlock::get_next_index()),
        block_(NULL), generated_(false), error_(false),
        source_id_(0), source_(false), flags_(0),
        ontology_(0), metadata_(NULL), metadata_len_(0) {}
    
    MetadataBlock(BlockInfo *block): lock_("MetadataBlock"),
        id_(MetadataBlock::get_next_index()),
        block_(block), generated_(false), error_(false),
        source_id_(0), source_(false), flags_(0),
        ontology_(0), metadata_(NULL), metadata_len_(0) {}
    
    MetadataBlock(u_int64_t type, u_char* buf, u_int32_t len);
    
    MetadataBlock(unsigned int source_id, u_int64_t type,
                  u_char* buf, u_int32_t len);
    
    MetadataBlock(oasys::Builder& builder): lock_("MetadataBlock"),
        id_(MetadataBlock::get_next_index()),
        block_(NULL), generated_(false), error_(false),
        source_id_(0), source_(false), flags_(0),
        ontology_(0), metadata_(NULL), metadata_len_(0)
        { (void)builder; }
    
    MetadataBlock(const MetadataBlock& copy);

    // Destructor
    ~MetadataBlock();

    // Accessors
    unsigned int id()           const { return id_; }
    bool         generated()    const { return generated_; }
    bool         error()        const { return error_; }
    unsigned int source_id()    const { return source_id_; }
    bool         source()       const { return source_; }
    u_int64_t    flags()        const { return flags_; }
    u_int64_t    ontology()     const { return ontology_; }
    u_char *     metadata()     const { return metadata_; }
    u_int32_t    metadata_len() const { return metadata_len_; }

    oasys::Lock* lock() { return &lock_; }

    // Set class data.
    void set_flags(u_int64_t flags);
    void set_block_error()                { error_ = true; }
    void set_ontology(u_int64_t ontology) { ontology_ = ontology; }
    void set_metadata(u_char *buf, u_int32_t len);
    
    /**
     * Mark the metadata block for removal from the bundle on the
     * specified outgoing link.
     *
     * @returns true if the metadata block was successfully marked for
     * removal; otherwise false.
     */
    bool remove_outgoing_metadata(const LinkRef& link);

    /**
     * Modify the metadata block for the specified outgoing link.
     *
     * @returns true if the metadata block was successfully modified;
     * otherwise false.
     */
    bool modify_outgoing_metadata(const LinkRef& link,
                                  u_char* buf, u_int32_t len);

    /**
     * @returns true if the metadata block is marked for removal from
     * from the bundle on the specified outgoing link; otherwise false.
     */
    bool metadata_removed(const LinkRef& link);

    /**
     * @returns true if the metadata block is modified for the specified
     * outgoing link; otherwise false.
     */
    bool metadata_modified(const LinkRef& link);

    /**
     * @returns true if the metadata block is modified for the specified
     * outgoing link; otherwise false. The modified ontology data and
     * length are returned if the metadata block was modified.
     */
    bool metadata_modified(const LinkRef& link, u_char** buf, u_int32_t& len);

    /**
     * Remove any outgoing metadata state for the specified link.
     */
    void delete_outgoing_metadata(const LinkRef& link);

    /// Virtual from SerializableObject
    virtual void serialize(oasys::SerializeAction* action) { (void)action; }
    
    void operator=(const MetadataBlock& copy);

    static unsigned int get_next_index() { return index_++; }

private:
    oasys::Mutex lock_;

    unsigned int id_;                ///< unique identifier
    BlockInfo *  block_;             ///< corresponding BlockInfo

    bool         generated_;         ///< flag that indicates if the metadata
                                     ///< was locally generated, and therefore
                                     ///< memory allocated to the ontology buf

    bool         error_;             ///< flag that indicates if the metadata
                                     ///< block was received with errors

    unsigned int source_id_;
    bool         source_;            // flag that indicates if the block
                                     // is a link-specific modification
                                     // of metadata generated at the API

    u_int64_t     flags_;            // flags specified at the API for
                                     // generated metadata

    u_int64_t    ontology_;
    u_char *     metadata_;
    u_int32_t    metadata_len_;

    class OutgoingMetadata {
    public:
        // Constructors
        OutgoingMetadata(const LinkRef& link):
            link_(link.object(), "OutgoingMetadata"), remove_(true),
            metadata_(NULL), metadata_len_(0) {}
        OutgoingMetadata(const LinkRef& link, u_char* buf, u_int32_t len);
        OutgoingMetadata(const OutgoingMetadata& copy);

        // Destructor
        ~OutgoingMetadata();

        void operator=(const OutgoingMetadata& copy);

        // Accessors
        const LinkRef& link()    const { return link_; }
        bool      remove()       const { return remove_; }
        u_char *  metadata()     const { return metadata_; }
        u_int32_t metadata_len() const { return metadata_len_; }

    private:
        LinkRef   link_;
        bool      remove_;
        u_char *  metadata_;
        u_int32_t metadata_len_;
    };

    std::vector<OutgoingMetadata> outgoing_metadata_;

    OutgoingMetadata* find_outgoing_metadata(const LinkRef& link);
    bool              has_outgoing_metadata(const LinkRef& link)
                          { return (find_outgoing_metadata(link) != NULL); }

    static unsigned int index_;
};

/**
 * Typedef for a reference to a MetadataBlock.
 */
typedef oasys::Ref<MetadataBlock> MetadataBlockRef;

/**
 * A vector of Metadata Block references.
 */
class MetadataVec : public std::vector<MetadataBlockRef> {
public:
    MetadataVec(const std::string& name) : name_(name) {}

    /**
     * Wrapper around push_back that takes a vanilla MetadataBlock
     * pointer.
     */
    void push_back(MetadataBlock* block) {
        std::vector<MetadataBlockRef>::
            push_back(MetadataBlockRef(block, name_.c_str()));
    }

protected:
    std::string name_;
};

/**
 * Data structure to store a metadata block vector for each outgoing
 * link, similar to LinkBlockSet.
 */
class LinkMetadataSet {
public:
    // Destructor
    virtual ~LinkMetadataSet();
    
    MetadataVec* create_blocks(const LinkRef& link);
    MetadataVec* find_blocks(const LinkRef& link) const;
    void         delete_blocks(const LinkRef& link);

private:
    struct Entry {
        Entry(const LinkRef& link);

        MetadataVec* blocks_;
        LinkRef      link_;
    };

    typedef std::vector<Entry> Vector;
    typedef std::vector<Entry>::iterator iterator;
    typedef std::vector<Entry>::const_iterator const_iterator;
    Vector entries_;
};

} // namespace dtn

#endif /* _METADATA_BLOCK_H_ */
