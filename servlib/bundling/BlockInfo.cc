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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/debug/Log.h>
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "BP6_APIBlockProcessor.h"
#include "BP6_UnknownBlockProcessor.h"
//#include "BP7_APIBlockProcessor.h"
#include "BP7_UnknownBlockProcessor.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion6.h"
#include "BundleProtocolVersion7.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
BlockInfo::BlockInfo(BlockProcessor* owner)
    : SerializableObject(),
      owner_(owner),
      owner_type_(owner->block_type()),
      source_(nullptr),
      eid_list_(),
      contents_(),
      locals_("BlockInfo constructor"),
      block_number_(0),
      block_flags_(0),
      data_length_(0),
      data_offset_(0),
      complete_(false),
      reloaded_(false),
      crc_type_(0),
      crc_length_(0),
      crc_offset_(0),
      crc_bytes_received_(0),
      crc_checked_(false),
      crc_validated_(false),
      bpv7_EOB_offset_(0)
{
    eid_list_.clear();
    memset(crc_cbor_bytes_, 0, sizeof(crc_cbor_bytes_));
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(BlockProcessor* owner, const SPtr_BlockInfo source)
    : SerializableObject(),
      owner_(owner),
      owner_type_(owner->block_type()),
      source_(source),
      eid_list_(),
      contents_(),
      locals_("BlockInfo constructor"),
      block_number_(0),
      block_flags_(0),
      data_length_(0),
      data_offset_(0),
      complete_(false),
      reloaded_(false),
      crc_type_(0),
      crc_length_(0),
      crc_offset_(0),
      crc_bytes_received_(0),
      crc_checked_(false),
      crc_validated_(false),
      bpv7_EOB_offset_(0)
{
    eid_list_.clear();
    memset(crc_cbor_bytes_, 0, sizeof(crc_cbor_bytes_));
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(oasys::Builder& builder)
    : SerializableObject(),
      owner_(nullptr),
      owner_type_(0),
      source_(nullptr),
      eid_list_(),
      contents_(),
      locals_("BlockInfo constructor"),
      block_number_(0),
      block_flags_(0),
      data_length_(0),
      data_offset_(0),
      complete_(false),
      reloaded_(true),
      crc_type_(0),
      crc_length_(0),
      crc_offset_(0),
      crc_bytes_received_(0),
      crc_checked_(false),
      crc_validated_(false),
      bpv7_EOB_offset_(0)
{
    (void)builder;

    memset(crc_cbor_bytes_, 0, sizeof(crc_cbor_bytes_));
}

//----------------------------------------------------------------------
BlockInfo::BlockInfo(const BlockInfo &bi)
    : SerializableObject(),
      owner_(bi.owner_),
      owner_type_(bi.owner_type_),
      source_(bi.source_),
      eid_list_(bi.eid_list_),
      contents_(bi.contents_),
      locals_(bi.locals_.object(), "BlockInfo copy constructor"),
      block_number_(0),
      block_flags_(0),
      data_length_(bi.data_length_),
      data_offset_(bi.data_offset_),
      complete_(bi.complete_),
      reloaded_(bi.reloaded_),
      crc_type_(bi.crc_type_),
      crc_length_(bi.crc_length_),
      crc_offset_(bi.crc_offset_),
      crc_bytes_received_(bi.crc_bytes_received_),
      crc_checked_(bi.crc_checked_),
      crc_validated_(bi.crc_validated_),
      bpv7_EOB_offset_(bi.bpv7_EOB_offset_)
{
    memcpy(crc_cbor_bytes_, bi.crc_cbor_bytes_, sizeof(crc_cbor_bytes_));
}


//----------------------------------------------------------------------
BlockInfo::BlockInfo(const SPtr_BlockInfo& bi)
    : SerializableObject(),
      owner_(bi->owner_),
      owner_type_(bi->owner_type_),
      source_(bi->source_),
      eid_list_(bi->eid_list_),
      contents_(bi->contents_),
      locals_(bi->locals_.object(), "BlockInfo copy constructor"),
      block_number_(0),
      block_flags_(0),
      data_length_(bi->data_length_),
      data_offset_(bi->data_offset_),
      complete_(bi->complete_),
      reloaded_(bi->reloaded_),
      crc_type_(bi->crc_type_),
      crc_length_(bi->crc_length_),
      crc_offset_(bi->crc_offset_),
      crc_bytes_received_(bi->crc_bytes_received_),
      crc_checked_(bi->crc_checked_),
      crc_validated_(bi->crc_validated_),
      bpv7_EOB_offset_(bi->bpv7_EOB_offset_)
{
    memcpy(crc_cbor_bytes_, bi->crc_cbor_bytes_, sizeof(crc_cbor_bytes_));
}

//----------------------------------------------------------------------
BlockInfo::~BlockInfo()
{
}

//----------------------------------------------------------------------
// Copy assignment operator
BlockInfo& BlockInfo::operator=(const BlockInfo& other)
{
    owner_ = other.owner_;
    owner_type_ = other.owner_type_;
    source_ = other.source_;
    eid_list_ = other.eid_list_;

    contents_.clear();
    contents_.reserve(other.contents_.buf_len());
    memcpy(contents_.buf(), other.contents_.buf(), other.contents_.buf_len());
    contents_.set_len(other.contents_.len());

    locals_ = other.locals_;
    data_length_ = other.data_length_;
    data_offset_ = other.data_offset_;
    complete_ = other.complete_;
    reloaded_ = other.reloaded_;

    block_number_ = other.block_number_;
    block_flags_ = other.block_flags_;
    crc_type_ = other.crc_type_;
    crc_length_ = other.crc_length_;
    crc_offset_ = other.crc_offset_;
    crc_bytes_received_ = other.crc_bytes_received_;
    crc_checked_ = other.crc_checked_;
    crc_validated_ = other.crc_validated_;
    bpv7_EOB_offset_ = other.bpv7_EOB_offset_;
    memcpy(crc_cbor_bytes_, other.crc_cbor_bytes_, sizeof(crc_cbor_bytes_));

    return *this;
}


//----------------------------------------------------------------------
// Move assignment operator
BlockInfo& BlockInfo::operator=(BlockInfo&& other) noexcept
{
    if (this != &other)
    {
        owner_ = other.owner_;
        owner_type_ = other.owner_type_;
        source_ = other.source_;
        eid_list_ = other.eid_list_;

        contents_.clear();
        contents_.reserve(other.contents_.buf_len());
        memcpy(contents_.buf(), other.contents_.buf(), other.contents_.buf_len());
        contents_.set_len(other.contents_.len());
        other.contents_.clear();
  
        locals_ = other.locals_;
        data_length_ = other.data_length_;
        data_offset_ = other.data_offset_;
        complete_ = other.complete_;
        reloaded_ = other.reloaded_;

        block_number_ = other.block_number_;
        block_flags_ = other.block_flags_;
        crc_type_ = other.crc_type_;
        crc_length_ = other.crc_length_;
        crc_offset_ = other.crc_offset_;
        crc_bytes_received_ = other.crc_bytes_received_;
        crc_checked_ = other.crc_checked_;
        crc_validated_ = other.crc_validated_;
        bpv7_EOB_offset_ = other.bpv7_EOB_offset_;
        memcpy(crc_cbor_bytes_, other.crc_cbor_bytes_, sizeof(crc_cbor_bytes_));
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
    // Note: Primary Block type is zero for both BPv6 and BPv7
    if (owner_->block_type() == BundleProtocolVersion7::PRIMARY_BLOCK) {
        return BundleProtocolVersion7::PRIMARY_BLOCK;
    }

    if (contents_.len() == 0) {
        if (owner_ != nullptr)
            return owner_->block_type();
        
        // Note: Unknown Block type is the same for both BPv6 and BPv7
        return BundleProtocolVersion7::UNKNOWN_BLOCK;
    }

    u_char* data = contents_.buf();
    (void)data;

    if ((data[0] == BundleProtocolVersion7::BP7_CBOR_BLOCK_ARRAY_NO_CRC) ||
        (data[0] == BundleProtocolVersion7::BP7_CBOR_BLOCK_ARRAY_WITH_CRC))
    {
        // this is a BP7 CBOR encoded block - the block type should be encoded in the 2nd or 3rd byte
        if (contents_.len() >= 2)
        {
            if ((data[1] & 0xe0) == 0)
            {
                // correct CBOR type for UnsignedIntegers
                if (data[1] < 0x18)
                {
                    // this is the actual block type value
                    return data[1];
                }
                else if (data[1] == 0x18)
                {
                    // block type value is the next byte
                    if (contents_.len() >= 3)
                    {
                        return data[2];
                    }
                    else
                    {
                        return BundleProtocolVersion7::UNKNOWN_BLOCK;
                    }
                }
                else
                {
                    // block type should be a 1 byte value so anything else is invalid/unknown
                    return BundleProtocolVersion7::UNKNOWN_BLOCK;
                }
            }
        }
        else
        {
            // not enough data to know yet
            return BundleProtocolVersion7::UNKNOWN_BLOCK;
        }
    }

    // fall through for BPv6 where the first byte is the Block Type
    return contents_.buf()[0];
}

//----------------------------------------------------------------------
u_int64_t
BlockInfo::block_flags() const
{
    u_int64_t flags = 0;

    if (owner()->is_bpv6()) {
        if (type() == BundleProtocolVersion6::PRIMARY_BLOCK) {
            return 0x0;
        }
        
        int sdnv_size = SDNV::decode(contents_.buf() + 1, contents_.len() - 1, 
                                 &flags);
        ASSERT(sdnv_size > 0);
    } else if (owner()->is_bpv7()) {
        flags = block_flags_;
    }

    return flags;
}

//----------------------------------------------------------------------
void
BlockInfo::set_flag(u_int64_t flag)
{
    block_flags_ = flag;

    if (owner()->is_bpv6()) {
        size_t sdnv_len = SDNV::encoding_len(flag);
        ASSERT(contents_.len() >= 1 + sdnv_len);
        SDNV::encode(flag, contents_.buf() + 1, sdnv_len);
    }
}

//----------------------------------------------------------------------
bool
BlockInfo::last_block() const
{
    if (owner()->is_bpv6()) {
        //check if it's too small to be flagged as last
        if (contents_.len() < 2) {
            return false;
        }
    
        u_int64_t flag = block_flags();
        return ((flag & BundleProtocolVersion6::BLOCK_FLAG_LAST_BLOCK) != 0);

    } else if (owner()->is_bpv7()) {
        return (owner_type_ == BundleProtocolVersion7::PAYLOAD_BLOCK);

    } else {
        return false;
    }
}

//----------------------------------------------------------------------
// return -1 = error; else total crc bytes received so far
int
BlockInfo::add_crc_cbor_bytes(uint8_t* buf, size_t buflen)
{
    if (buflen > (crc_length_ - crc_bytes_received_))
    {
        return -1;
    }
    else
    {
        memcpy(crc_cbor_bytes_ + crc_bytes_received_, buf, buflen);
        crc_bytes_received_ += buflen;
    }

    return crc_bytes_received_;
}


//----------------------------------------------------------------------
void
BlockInfo::serialize(oasys::SerializeAction* a)
{
    uint64_t bp_version = 0;
    if (owner() != nullptr) {
        bp_version = owner()->bp_version();
    }

    a->process("bp_version", &bp_version);
    
    a->process("owner_type", &owner_type_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        if (bp_version == BundleProtocol::BP_VERSION_6) {
            // need to re-assign the owner
            if (owner_type_ == BundleProtocolVersion6::API_EXTENSION_BLOCK) {
                owner_ = BP6_APIBlockProcessor::instance();
            } else if (owner_type_ == BundleProtocolVersion6::UNKNOWN_BLOCK) {
                owner_ = BP6_UnknownBlockProcessor::instance();
            } else {
                owner_ = BundleProtocolVersion6::find_processor(owner_type_);
            }
        } else if (bp_version == BundleProtocol::BP_VERSION_7) {
            // need to re-assign the owner
            if (owner_type_ == BundleProtocolVersion7::UNKNOWN_BLOCK) {
                owner_ = BP7_UnknownBlockProcessor::instance();
            } else {
                owner_ = BundleProtocolVersion7::find_processor(owner_type_);
            }
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
    a->process("block_number", &block_number_);
    a->process("block_flags", &block_flags_);
    a->process("crc_type", &crc_type_);
    a->process("crc_length", &crc_length_);
    a->process("crc_offset", &crc_offset_);
    a->process("crc_checkd", &crc_checked_);
    a->process("crc_validated", &crc_validated_);
    a->process("bpv7_EOB_offset", &bpv7_EOB_offset_);
    a->process("crc_cbor_bytes", crc_cbor_bytes_, sizeof(crc_cbor_bytes_));
}

//----------------------------------------------------------------------
BlockInfoVec::BlockInfoVec()
    : oasys::SerializableSPtrVector<BlockInfo>(),
      error_major_(0),
      error_minor_(0),
      error_debug_(0)
{
}

//----------------------------------------------------------------------
SPtr_BlockInfo
BlockInfoVec::append_block(BlockProcessor* owner)
{
    SPtr_BlockInfo blkptr = std::make_shared<BlockInfo>(owner);
    push_back(blkptr);
    return blkptr;
}

//----------------------------------------------------------------------
SPtr_BlockInfo
BlockInfoVec::append_block(BlockProcessor* owner, const SPtr_BlockInfo source)
{
    SPtr_BlockInfo blkptr = std::make_shared<BlockInfo>(owner, source);
    push_back(blkptr);
    return blkptr;
}

//----------------------------------------------------------------------
SPtr_BlockInfo
BlockInfoVec::find_block(u_int8_t type) const
{
    SPtr_BlockInfo blkptr;

    for (const_iterator iter = begin(); iter != end(); ++iter) {
        blkptr = *iter;
        if ((blkptr->type() == type) || (blkptr->owner()->block_type() == type))
        {
            break;
        }

        blkptr = nullptr;
    }
    return blkptr;
}

//----------------------------------------------------------------------
LinkBlockSet::Entry::Entry(const LinkRef& link)
    : blocks_(nullptr), link_(link.object(), "LinkBlockSet::Entry")
{
}

//----------------------------------------------------------------------
LinkBlockSet::~LinkBlockSet()
{
}

//----------------------------------------------------------------------
SPtr_BlockInfoVec
LinkBlockSet::create_blocks(const LinkRef& link)
{
    oasys::ScopeLock l(lock_, "LinkBlockSet::create_blocks");
    
    ASSERT(find_blocks(link) == nullptr);
    entries_.push_back(Entry(link));
    entries_.back().blocks_ = std::make_shared<BlockInfoVec>();
    return entries_.back().blocks_;
}

//----------------------------------------------------------------------
SPtr_BlockInfoVec
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
    return nullptr;
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
            entries_.erase(iter);
            return;
        }
    }
}

} // namespace dtn
