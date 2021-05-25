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

#include <sys/types.h>
#include <netinet/in.h>

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/util/StringUtils.h>

#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion6.h"
#include "BundleProtocolVersion7.h"

namespace dtn {

BundleProtocol::Params::Params()
    : status_rpts_enabled_(false),
       use_bundle_age_block_(false),
       default_hop_limit_(0)
{}

BundleProtocol::Params BundleProtocol::params_;

//----------------------------------------------------------------------
void
BundleProtocol::init_default_processors()
{
    BundleProtocolVersion6::init_default_processors();
    BundleProtocolVersion7::init_default_processors();
}

//----------------------------------------------------------------------
void
BundleProtocol::reload_post_process(Bundle* bundle)
{
    if (bundle->is_bpv6())
    {
        BundleProtocolVersion6::reload_post_process(bundle);
    }
    else
    {
        BundleProtocolVersion7::reload_post_process(bundle);
    } 
}
        
//----------------------------------------------------------------------
SPtr_BlockInfoVec
BundleProtocol::prepare_blocks(Bundle* bundle, const LinkRef& link)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::prepare_blocks(bundle, link);
    }
    else
    {
        return BundleProtocolVersion7::prepare_blocks(bundle, link);
    } 
}

//----------------------------------------------------------------------
size_t
BundleProtocol::total_length(Bundle* bundle, const BlockInfoVec* blocks)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::total_length(bundle, blocks);
    }
    else
    {
        return BundleProtocolVersion7::total_length(bundle, blocks);
    } 
}

//----------------------------------------------------------------------
size_t
BundleProtocol::payload_offset(const Bundle* bundle, const BlockInfoVec* blocks)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::payload_offset(blocks);
    }
    else
    {
        return BundleProtocolVersion7::payload_offset(blocks);
    } 
}

//----------------------------------------------------------------------
size_t
BundleProtocol::generate_blocks(Bundle*        bundle,
                                BlockInfoVec*  blocks,
                                const LinkRef& link)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::generate_blocks(bundle, blocks, link);
    }
    else
    {
        return BundleProtocolVersion7::generate_blocks(bundle, blocks, link);
    } 
}

//----------------------------------------------------------------------
void
BundleProtocol::delete_blocks(Bundle* bundle, const LinkRef& link)
{
    if (bundle->is_bpv6())
    {
        BundleProtocolVersion6::delete_blocks(bundle, link);
    }
    else
    {
        BundleProtocolVersion7::delete_blocks(bundle, link);
    } 
}

//----------------------------------------------------------------------
size_t
BundleProtocol::produce(const Bundle* bundle, const BlockInfoVec* blocks,
                        u_char* data, size_t offset, size_t len, bool* last)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::produce(bundle, blocks, data, offset, len, last);
    }
    else
    {
        return BundleProtocolVersion7::produce(bundle, blocks, data, offset, len, last);
    } 
}
    
//----------------------------------------------------------------------
ssize_t
BundleProtocol::consume(Bundle* bundle,
                        u_char* data,
                        size_t  len,
                        bool*   last)
{
    if ((data == nullptr) || (len == 0))
    {
        return 0;
    }

    if (bundle->is_bpv_unknown())
    {
        // check the first byte of data to determine most likely version
        if (data[0] == 0x06)  // SDNV value for BP version 6
        {
            bundle->set_bp_version(BundleProtocol::BP_VERSION_6);
        }
        else
        {
            bundle->set_bp_version(BundleProtocol::BP_VERSION_7);
        }
    }

    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::consume(bundle, data, len, last);
    }
    else
    {
        return BundleProtocolVersion7::consume(bundle, data, len, last);
    } 
}

//----------------------------------------------------------------------
bool
BundleProtocol::validate(Bundle* bundle,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    if (bundle->is_bpv6())
    {
        return BundleProtocolVersion6::validate(bundle, reception_reason, deletion_reason);
    }
    else
    {
        return BundleProtocolVersion7::validate(bundle, reception_reason, deletion_reason);
    } 
}

} // namespace dtn
