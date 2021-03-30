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

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/debug/Logger.h>
#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/storage/StoreDetail.h>

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
}
//----------------------------------------------------------------------
BundleDetail::BundleDetail(const oasys::Builder&) :    
		StoreDetail("BundleDetail", "/dtn/bundle/bundle_detail", oasys::DGP_PUT),
		bundle_(NULL)
{
	bundleid_ = BUNDLE_ID_MAX;
}
//----------------------------------------------------------------------
BundleDetail::~BundleDetail()
{
}
//----------------------------------------------------------------------
}  /* namespace dtn */

#endif /* defined LIBODBC_ENABLED */
