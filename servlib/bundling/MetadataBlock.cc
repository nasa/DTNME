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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/Log.h>

#include "MetadataBlock.h"
#include "MetadataBlockProcessor.h"

namespace dtn {

unsigned int MetadataBlock::index_ = 0;

//----------------------------------------------------------------------
MetadataBlock::MetadataBlock(u_int64_t type, u_char* buf, u_int32_t len):
    lock_("MetadataBlock"), id_(MetadataBlock::get_next_index()),
    block_(NULL), generated_(true), error_(false),
    source_id_(0), source_(false), flags_(0),
    ontology_(type), metadata_(NULL), metadata_len_(0)
{
    if (len > 0) {
        ASSERT(buf != NULL);
        metadata_ = new u_char[len];
        memcpy(metadata_, buf, len);
        metadata_len_ = len;
    }
}

//----------------------------------------------------------------------
MetadataBlock::MetadataBlock(unsigned int source_id, u_int64_t type,
                             u_char* buf, u_int32_t len):
    lock_("MetadataBlock"), id_(MetadataBlock::get_next_index()),
    block_(NULL), generated_(true), error_(false),
    source_id_(source_id), source_(true), flags_(0),
    ontology_(type), metadata_(NULL), metadata_len_(0)
{
    if (len > 0) {
        ASSERT(buf != NULL);
        metadata_ = new u_char[len];
        memcpy(metadata_, buf, len);
        metadata_len_ = len;
    }
}

//----------------------------------------------------------------------
MetadataBlock::MetadataBlock(const MetadataBlock& copy):
    BP_Local(copy),
    oasys::SerializableObject(copy),
    lock_("MetadataBlock"), id_(copy.id_), block_(copy.block_),
    generated_(copy.generated_), error_(copy.error_),
    source_id_(copy.source_id_), source_(copy.source_),
    ontology_(copy.ontology_), metadata_(NULL),
    metadata_len_(0)
{
    if (copy.generated_ && (copy.metadata_len_ > 0)) {
        ASSERT(copy.metadata_ != NULL);
        metadata_ = new u_char[copy.metadata_len_];
        memcpy(metadata_, copy.metadata_, copy.metadata_len_);
        metadata_len_ = copy.metadata_len_;

    } else {
        metadata_     = copy.metadata_;
        metadata_len_ = copy.metadata_len_;
    }
}

//----------------------------------------------------------------------
MetadataBlock::~MetadataBlock()
{
    outgoing_metadata_.clear();
    if (generated_ && (metadata_ != NULL)) {
        delete[] metadata_;
        metadata_ = NULL;
        metadata_len_ = 0;
    }
}

//----------------------------------------------------------------------
void
MetadataBlock::set_flags(u_int64_t flags)
{
    ASSERT(generated_);
    flags_ = flags;
}

//----------------------------------------------------------------------
void
MetadataBlock::set_metadata(u_char *buf, u_int32_t len)
{
    //ASSERT(!generated_);

    if (len > 0) {
        ASSERT(buf != NULL);
        metadata_ = buf;
        metadata_len_ = len;
    } else {
        metadata_ = NULL;
        metadata_len_ = 0;
    }
}

//----------------------------------------------------------------------
bool
MetadataBlock::remove_outgoing_metadata(const LinkRef& link)
{
    static const char* log = "/dtn/bundle/protocol";

    if (has_outgoing_metadata(link)) {
        log_err_p(log, "MetadataBlock::remove_outgoing_metadata: "
                       "outgoing metadata already exists for link");
        return false;
    }

    outgoing_metadata_.push_back(OutgoingMetadata(link));
    return true;
}

//----------------------------------------------------------------------
bool
MetadataBlock::modify_outgoing_metadata(const LinkRef& link,
                                        u_char* buf, u_int32_t len)
{
    static const char* log = "/dtn/bundle/protocol";

    if (has_outgoing_metadata(link)) {
        log_err_p(log, "MetadataBlock::modify_outgoing_metadata: "
                       "outgoing metadata already exists for link");
        return false;
    }

    outgoing_metadata_.push_back(OutgoingMetadata(link, buf, len));
    return true;
}

//----------------------------------------------------------------------
bool
MetadataBlock::metadata_removed(const LinkRef& link)
{
    OutgoingMetadata* metadata = find_outgoing_metadata(link);
    if (metadata == NULL) {
        return false;
    }

    return (metadata->remove());
}

//----------------------------------------------------------------------
bool
MetadataBlock::metadata_modified(const LinkRef& link)
{
    OutgoingMetadata* metadata = find_outgoing_metadata(link);
    if ((metadata == NULL) || metadata->remove()) {
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
bool
MetadataBlock::metadata_modified(const LinkRef& link,
                                 u_char** buf, u_int32_t& len)
{
    OutgoingMetadata* outgoing = find_outgoing_metadata(link);
    if ((outgoing == NULL) || outgoing->remove()) {
        return false;
    }

    *buf = outgoing->metadata();
    len  = outgoing->metadata_len();
    return true;
}

//----------------------------------------------------------------------
void
MetadataBlock::delete_outgoing_metadata(const LinkRef& link)
{
    std::vector<OutgoingMetadata>::iterator iter = outgoing_metadata_.begin();
    for ( ; iter != outgoing_metadata_.end(); ++iter) {
        if (iter->link() == link) {
            outgoing_metadata_.erase(iter);
            return;
        }
    }
}

//----------------------------------------------------------------------
void
MetadataBlock::operator=(const MetadataBlock& copy)
{
    if (&copy == this) {
        return;
    }

    outgoing_metadata_.clear();
    if (generated_ && (metadata_ != NULL)) {
        delete[] metadata_;
        metadata_ = NULL;
        metadata_len_ = 0;
    }

    id_        = copy.id_;
    block_     = copy.block_;
    generated_ = copy.generated_;
    error_     = copy.error_;
    ontology_  = copy.ontology_;

    if (copy.generated_ && (copy.metadata_len_ > 0)) {
        ASSERT(copy.metadata_ != NULL);
        metadata_ = new u_char[copy.metadata_len_];
        memcpy(metadata_, copy.metadata_, copy.metadata_len_);
        metadata_len_ = copy.metadata_len_;

    } else {
        metadata_     = copy.metadata_;
        metadata_len_ = copy.metadata_len_;
    }
}

//----------------------------------------------------------------------
MetadataBlock::OutgoingMetadata *
MetadataBlock::find_outgoing_metadata(const LinkRef& link)
{
    std::vector<OutgoingMetadata>::iterator iter = outgoing_metadata_.begin();
    for ( ; iter != outgoing_metadata_.end(); ++iter) {
        if (iter->link() == link) {
            return &*iter;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
MetadataBlock::OutgoingMetadata::OutgoingMetadata(
    const LinkRef& link, u_char* buf, u_int32_t len):
    link_(link.object(), "OutgoingMetadata"), remove_(false),
    metadata_(NULL), metadata_len_(0)
{
    if (len > 0) {
        ASSERT(buf != NULL);
        metadata_ = new u_char[len];
        memcpy(metadata_, buf, len);
        metadata_len_ = len;
    }
}

//----------------------------------------------------------------------
MetadataBlock::OutgoingMetadata::OutgoingMetadata(const OutgoingMetadata& copy):
    link_(copy.link_.object(), "OutgoingMetadata"), remove_(copy.remove_),
    metadata_(NULL), metadata_len_(0)
{
    if (copy.metadata_len_ > 0) {
        ASSERT(copy.metadata_ != NULL);
        metadata_ = new u_char[copy.metadata_len_];
        memcpy(metadata_, copy.metadata_, copy.metadata_len_);
        metadata_len_ = copy.metadata_len_;
    }
}

//----------------------------------------------------------------------
MetadataBlock::OutgoingMetadata::~OutgoingMetadata()
{
    if (metadata_ != NULL) {
        delete[] metadata_;
        metadata_ = NULL;
        metadata_len_ = 0;
    }
}

//----------------------------------------------------------------------
void
MetadataBlock::OutgoingMetadata::operator=(const OutgoingMetadata& copy)
{
    if (&copy == this) {
        return;
    }

    if (metadata_ != NULL) {
        delete[] metadata_;
        metadata_ = NULL;
        metadata_len_ = 0;
    }

    link_ = copy.link_;
    remove_ = copy.remove_;

    if (copy.metadata_len_ > 0) {
        ASSERT(copy.metadata_ != NULL);
        metadata_ = new u_char[copy.metadata_len_];
        memcpy(metadata_, copy.metadata_, copy.metadata_len_);
        metadata_len_ = copy.metadata_len_;
    }
}

//----------------------------------------------------------------------
LinkMetadataSet::Entry::Entry(const LinkRef& link):
    blocks_(NULL), link_(link.object(), "LinkMetadataSet::Entry")
{
}

//----------------------------------------------------------------------
LinkMetadataSet::~LinkMetadataSet()
{
    for (iterator iter = entries_.begin(); iter != entries_.end(); ++iter) {
        delete iter->blocks_;
        iter->blocks_ = NULL;
    }
}

//----------------------------------------------------------------------
MetadataVec*
LinkMetadataSet::create_blocks(const LinkRef& link)
{
    ASSERT(find_blocks(link) == NULL);
    entries_.push_back(Entry(link));
    entries_.back().blocks_ =
        new MetadataVec(std::string("metadata for link ") +
                        ((link == NULL) ? "(null link)" : link->name()));
    return entries_.back().blocks_;
}

//----------------------------------------------------------------------
MetadataVec*
LinkMetadataSet::find_blocks(const LinkRef& link) const
{
    for (const_iterator iter = entries_.begin();
         iter != entries_.end(); ++iter) {
        if (iter->link_ == link) {
            return iter->blocks_;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
void
LinkMetadataSet::delete_blocks(const LinkRef& link)
{
    for (iterator iter = entries_.begin(); iter != entries_.end(); ++iter) {
        if (iter->link_ == link) {
            delete iter->blocks_;
            iter->blocks_ = NULL;
            entries_.erase(iter);
            return;
        }
    }
}

} // namespace dtn
