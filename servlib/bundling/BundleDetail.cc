/*
 *    Copyright 2004-2006 Intel Corporation
 *    Copyright 2011 Trinity College Dublin
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

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Logger.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/storage/StoreDetail.h>

#include "Bundle.h"
#include "BundleDetail.h"

// This functionality is only useful with ODBC/SQL  data storage
#ifdef LIBODBC_ENABLED

namespace dtn {

//----------------------------------------------------------------------
BundleDetail::BundleDetail(Bundle				   *bndl,
						   oasys::detail_get_put_t	op ):
		StoreDetail("BundleDetail", "/dtn/bundle/bundle_detail", op),
		bundle_(bndl)
{
	bundleid_ = bndl->bundleid();
	creation_time_ = bndl->creation_ts().seconds_;
	creation_seq_ = bndl->creation_ts().seqno_;
	payload_length_ = bndl->payload().length();
	log_debug("size of size_t: %d", sizeof(size_t));

        // XXX/dz how to handle 32 vs 64 bit systems and bundle id size?
        if (4 == sizeof(bundleid_)) {
    		add_detail("bundle_id",			oasys::DK_ULONG,
				   (void *)&bundleid_,	sizeof(bundleid_));
	} else {
		add_detail("bundle_id",			oasys::DK_ULONG_LONG,
				   (void *)&bundleid_,	sizeof(bundleid_));
	}

	add_detail("creation_time",		oasys::DK_ULONG_LONG,
			   (void *)&creation_time_, sizeof(u_int64_t));
	add_detail("creation_seq",		oasys::DK_ULONG_LONG,
			   (void *)&creation_seq_, sizeof(u_int64_t));
	add_detail("source_eid",		oasys::DK_VARCHAR,
			   (void *)const_cast<char *>(bndl->source().c_str()),
			   bndl->source().length());
	add_detail("dest_eid",			oasys::DK_VARCHAR,
			   (void *)const_cast<char *>(bndl->dest().c_str()),
			   bndl->dest().length());
	if (bndl->payload().location() == BundlePayload::DISK) {
		add_detail("payload_file",	oasys::DK_VARCHAR,
				   (void *)const_cast<char *>(bndl->payload().filename().c_str()),
				   bndl->payload().filename().length());
	} else {
		add_detail("payload_file",	oasys::DK_VARCHAR,		(void *)"in_memory", 9);
	}
	if ( bndl->is_fragment()) {
		fragment_offset_ = bndl->frag_offset();
		original_length_ = bndl->orig_length();

		add_detail("fragment_length",			oasys::DK_ULONG,
				   (void *)&payload_length_,		sizeof(size_t));
		add_detail("fragment_offset",			oasys::DK_ULONG,
				   (void *)&fragment_offset_,	sizeof(u_int32_t));
		add_detail("bundle_length",				oasys::DK_ULONG,
				   (void *)&original_length_,	sizeof(u_int32_t));
	} else {
		add_detail("bundle_length",				oasys::DK_ULONG,
				   (void *)&payload_length_,		sizeof(size_t));
	}

#ifdef BPQ_ENABLED
	/* Check if bundle has a BPQ block */
	has_BPQ_blk_ = false;
	const BlockInfo* bi_bpq;
    if( ((bi_bpq = bndl->recv_blocks().find_block(BundleProtocol::QUERY_EXTENSION_BLOCK)) != NULL) ||
        ((bi_bpq = (const_cast<Bundle*>(bndl)->api_blocks()->
                       find_block(BundleProtocol::QUERY_EXTENSION_BLOCK))) != NULL) ) {

        log_debug("bundle %d has BPQ block - adding details for BPQ", bundleid_);

        bpq_blk_ = dynamic_cast<BPQBlock *>(bi_bpq->locals());
        ASSERT (bpq_blk_ != NULL);
        has_BPQ_blk_ = true;
        bpq_blk_->add_ref("bundle_detail::", "add_detail");
        bpq_kind_ = bpq_blk_->kind();
        bpq_matching_rule_ = bpq_blk_->matching_rule();

		add_detail("bpq_kind",	oasys::DK_USHORT,
				   (void *)&bpq_kind_, sizeof(BPQBlock::kind_t));
		add_detail("bpq_matching_rule",	oasys::DK_USHORT,
				   (void *)&bpq_matching_rule_, sizeof(u_int));
		log_debug( "query: %s, src: %s", bpq_blk_->query_val(), bpq_blk_->source().c_str());
		add_detail("bpq_query", oasys::DK_VARCHAR,
				   (void *)const_cast<u_char *>(bpq_blk_->query_val()),
				   bpq_blk_->query_len() - 1);
		add_detail("bpq_real_source", oasys::DK_VARCHAR,
				   (void *)const_cast<char *>(bpq_blk_->source().c_str()),
				   bpq_blk_->source().length());
    }

#endif /* BPQ_ENABLED */
}
//----------------------------------------------------------------------
BundleDetail::BundleDetail(const oasys::Builder&) :    
		StoreDetail("BundleDetail", "/dtn/bundle/bundle_detail", oasys::DGP_PUT),
		bundle_(NULL)
{
	bundleid_ = BUNDLE_ID_MAX;
#ifdef BPQ_ENABLED
	has_BPQ_blk_ = false;
#endif /* BPQ_ENABLED */
}
//----------------------------------------------------------------------
BundleDetail::~BundleDetail()
{
#ifdef BPQ_ENABLED
	if (has_BPQ_blk_)
	{
		bpq_blk_->del_ref("bundle_detail:", "destructor");
	}
#endif /* BPQ_ENABLED */
}
//----------------------------------------------------------------------
}  /* namespace dtn */

#endif /* defined LIBODBC_ENABLED */
