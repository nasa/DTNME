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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/Log.h>
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "APIBlockProcessor.h"
#include "UnknownBlockProcessor.h"
#include "BundleProtocol.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
BlockInfo::BlockInfo(BlockProcessor* owner, const BlockInfo* source)
    : SerializableObject(),
#ifdef BSP_ENABLED
      original_block_type(0),
#endif
      owner_(owner),
      owner_type_(owner->block_type()),
      source_(source),
      eid_list_(),
      contents_(),
      locals_("BlockInfo constructor"),
      data_length_(0),
      data_offset_(0),
      complete_(false),
      reloaded_(false)

{
    eid_list_.clear();
#ifdef BSP_ENABLED
    if(source != NULL) {
        bsp = source->bsp;
    }
#endif
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(oasys::Builder& builder)
    : SerializableObject(),
#ifdef BSP_ENABLED
      original_block_type(0),
#endif
      owner_(NULL),
      owner_type_(0),
      source_(NULL),
      eid_list_(),
      contents_(),
      locals_("BlockInfo constructor"),
      data_length_(0),
      data_offset_(0),
      complete_(false),
      reloaded_(true)
{
    (void)builder;
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(const BlockInfo &bi)
    : SerializableObject(),
#ifdef BSP_ENABLED
      bsp(bi.bsp),
      original_block_type(bi.original_block_type),
#endif
      owner_(bi.owner_),
      owner_type_(bi.owner_type_),
      source_(bi.source_),
      eid_list_(bi.eid_list_),
      contents_(bi.contents_),
      locals_(bi.locals_.object(), "BlockInfo copy constructor"),
      data_length_(bi.data_length_),
      data_offset_(bi.data_offset_),
      complete_(bi.complete_),
      reloaded_(bi.reloaded_)
{
}

//----------------------------------------------------------------------
BlockInfo::~BlockInfo()
{
}


//----------------------------------------------------------------------
BlockInfo& BlockInfo::operator=(BlockInfo&& other) noexcept
{
  if (this != &other)
  {
#ifdef BSP_ENABLED
      bsp = other.bsp;
      original_block_type = other.original_block_type;
#endif
      owner_ = other.owner_;
      owner_type_ = other.owner_type_;
      source_ = other.source_;
      eid_list_ = other.eid_list_;

      //contents_ = other.contents_;
      contents_.clear();
      contents_.reserve(other.contents_.buf_len());
      memcpy(contents_.buf(), other.contents_.buf(), other.contents_.buf_len());

      locals_ = other.locals_;
      data_length_ = other.data_length_;
      data_offset_ = other.data_offset_;
      complete_ = other.complete_;
      reloaded_ = other.reloaded_;
  }
  return *this;
}


//----------------------------------------------------------------------
void
BlockInfo::set_locals(BP_Local* l)
{
    locals_ = l; 
}

//----------------------------------------------------------------------
int
BlockInfo::type() const
{
	if (owner_->block_type() == BundleProtocol::PRIMARY_BLOCK) {
        return BundleProtocol::PRIMARY_BLOCK;
    }

    if (contents_.len() == 0) {
        if (owner_ != NULL)
            return owner_->block_type();
        
        return BundleProtocol::UNKNOWN_BLOCK;
    }

    u_char* data = contents_.buf();
    (void)data;


/*
    if (owner_ != NULL)
        ASSERT(contents_.buf()[0] == owner_->block_type()
      //         || owner_->block_type() == BundleProtocol::CONFIDENTIALITY_BLOCK
      //         || owner_->block_type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK
      //         || owner_->block_type() == BundleProtocol::EXTENSION_SECURITY_BLOCK
                || owner_->block_type() == BundleProtocol::PREVIOUS_HOP_BLOCK
                || owner_->block_type() == BundleProtocol::METADATA_BLOCK
                || owner_->block_type() == BundleProtocol::EXTENSION_SECURITY_BLOCK
                || owner_->block_type() == BundleProtocol::SESSION_BLOCK
                || owner_->block_type() == BundleProtocol::AGE_BLOCK
                || owner_->block_type() == BundleProtocol::QUERY_EXTENSION_BLOCK
                || owner_->block_type() == BundleProtocol::SEQUENCE_ID_BLOCK
                || owner_->block_type() == BundleProtocol::OBSOLETES_ID_BLOCK
                || owner_->block_type() == BundleProtocol::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK
                || owner_->block_type() == BundleProtocol::EXTENDED_CLASS_OF_SERVICE_BLOCK
                || owner_->block_type() == BundleProtocol::UNKNOWN_BLOCK
                || owner_->block_type() == BundleProtocol::API_EXTENSION_BLOCK);
*/

    return contents_.buf()[0];
}

//----------------------------------------------------------------------
u_int64_t
BlockInfo::flags() const
{
    if (type() == BundleProtocol::PRIMARY_BLOCK) {
        return 0x0;
    }
    
    u_int64_t flags;
    int sdnv_size = SDNV::decode(contents_.buf() + 1, contents_.len() - 1, 
                                 &flags);
    ASSERT(sdnv_size > 0);
    return flags;
}

//----------------------------------------------------------------------
void
BlockInfo::set_flag(u_int64_t flag)
{
    size_t sdnv_len = SDNV::encoding_len(flag);
    ASSERT(contents_.len() >= 1 + sdnv_len);
    SDNV::encode(flag, contents_.buf() + 1, sdnv_len);
}

//----------------------------------------------------------------------
bool
BlockInfo::last_block() const
{
    //check if it's too small to be flagged as last
    if (contents_.len() < 2) {
        return false;
    }
    
    u_int64_t flag = flags();
    return ((flag & BundleProtocol::BLOCK_FLAG_LAST_BLOCK) != 0);
}

//----------------------------------------------------------------------
void
BlockInfo::serialize(oasys::SerializeAction* a)
{
    a->process("owner_type", &owner_type_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        // need to re-assign the owner
        if (owner_type_ == BundleProtocol::API_EXTENSION_BLOCK) {
            owner_ = APIBlockProcessor::instance();
        } else if (owner_type_ == BundleProtocol::UNKNOWN_BLOCK) {
            owner_ = UnknownBlockProcessor::instance();
        } else {
            owner_ = BundleProtocol::find_processor(owner_type_);
        }
    }

    u_int32_t length = contents_.len();
    a->process("length", &length);
    
    // when we're unserializing, we need to reserve space and set the
    // length of the contents buffer before we write into it
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        contents_.reserve(length);
        contents_.set_len(length);
    }

    a->process("contents", contents_.buf(), length);
    a->process("data_length", &data_length_);
    a->process("data_offset", &data_offset_);
    a->process("complete", &complete_);
    a->process("eid_list", &eid_list_);
#ifdef BSP_ENABLED
    a->process("bsp", &bsp);
    a->process("original_block_type", &original_block_type);
#endif
}

//----------------------------------------------------------------------
BlockInfoVec::BlockInfoVec()
    : oasys::SerializableVector<BlockInfo>(),
      error_major_(0),
      error_minor_(0),
      error_debug_(0)
{
}

//----------------------------------------------------------------------
BlockInfo*
BlockInfoVec::append_block(BlockProcessor* owner, const BlockInfo* source)
{
    push_back(BlockInfo(owner, source));
    return &back();
}

//----------------------------------------------------------------------
const BlockInfo*
BlockInfoVec::find_block(u_int8_t type) const
{
    for (const_iterator iter = begin(); iter != end(); ++iter) {
        if (iter->type() == type ||
            iter->owner()->block_type() == type)
        {
            return &*iter;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
LinkBlockSet::Entry::Entry(const LinkRef& link)
    : blocks_(NULL), link_(link.object(), "LinkBlockSet::Entry")
{
}

//----------------------------------------------------------------------
LinkBlockSet::~LinkBlockSet()
{
    for (iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        delete iter->blocks_;
        iter->blocks_ = 0;
    }
}

//----------------------------------------------------------------------
BlockInfoVec*
LinkBlockSet::create_blocks(const LinkRef& link)
{
    oasys::ScopeLock l(lock_, "LinkBlockSet::create_blocks");
    
    ASSERT(find_blocks(link) == NULL);
    entries_.push_back(Entry(link));
    entries_.back().blocks_ = new BlockInfoVec();
    return entries_.back().blocks_;
}

//----------------------------------------------------------------------
BlockInfoVec*
LinkBlockSet::find_blocks(const LinkRef& link) const
{
    oasys::ScopeLock l(lock_, "LinkBlockSet::find_blocks");
    
    for (const_iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        if (iter->link_ == link) {
            return iter->blocks_;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
void
LinkBlockSet::delete_blocks(const LinkRef& link)
{
    oasys::ScopeLock l(lock_, "LinkBlockSet::delete_blocks");
    
    for (iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter)
    {
        if (iter->link_ == link) {
            delete iter->blocks_;
            entries_.erase(iter);
            return;
        }
    }
    
    PANIC("LinkBlockVec::delete_blocks: "
          "no block vector for link *%p", link.object());
}

} // namespace dtn
