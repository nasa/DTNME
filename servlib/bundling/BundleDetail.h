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

#ifndef _BUNDLE_DETAIL_H_
#define _BUNDLE_DETAIL_H_

#include <oasys/serialize/Serialize.h>
#include <oasys/storage/StoreDetail.h>

#include "Bundle.h"

#ifdef BPQ_ENABLED
#include "BPQBlock.h"
#endif /* BPQ_ENABLED */



// This functionality is only useful with ODBC/SQL  data storage
#ifdef LIBODBC_ENABLED

namespace dtn {

/**
 * The class used to extract a number of fields from a Bundle that can be stored
 * separately in a database table to facilitate various operations, including
 * searching of a list of bundles t speed up forwarding with a large cache
 * and external access to bundles.  The exact fields of the bundle to be stored
 * are controlled by the setup list in the constructor.  The class is derived from
 * Serializable Object in order to allow it to be used in conjunction with the
 * Oasys DurableStore mechanisms, but in practice the data is *not* serialized
 * but is stored separately in columns of the bundle_details table (specified here).
 */



class BundleDetail : public oasys::StoreDetail

{

    public:
	    BundleDetail(Bundle 				   *bndl,
					 oasys::detail_get_put_t	op = oasys::DGP_PUT );
		/**
		 * Dummy constructor used only when doing get_table.
		 */
		BundleDetail(const oasys::Builder&);
	    ~BundleDetail();
        //void serialize(oasys::SerializeAction *a)   { (void)a; } ///<Do not use
	    /**
	     * Hook for the generic durable table implementation to know what
	     * the key is for the database.
	     */
	    bundleid_t durable_key() { return bundleid_; }


    private:
	    Bundle *		bundle_;			///< Bundle for which these details apply
	    /*
	     * These items are copies of items in the bundle as the private data from
	     * the bundle isn't exposed and we need stores that are persistent between
	     * method calls
	     */
	    bundleid_t bundleid_;			///< ID of the above bundle
		u_int64_t creation_time_;			///< UTC when (whole) bundle was created
		u_int64_t creation_seq_;			///< Sequence number of bundle in creating node
		size_t payload_length_;				///< Length of bundle payload (might be fragment)
		u_int32_t fragment_offset_;			///< Offset of fragment (if it is one)
		u_int32_t original_length_;			///< If a fragment, total length of bundle payload

#ifdef BPQ_ENABLED
		/*
		 * These items come from the BPQ block if they are compiled in and there is one
		 */
		bool has_BPQ_blk_;
		BPQBlock *bpq_blk_;					///< Copy of BPQ block
        BPQBlock::kind_t bpq_kind_;			///< The kind of BPQ block
        u_int bpq_matching_rule_;			///< The matching rule to apply
#endif /* BPQ_ENABLED */

};

}  /* namespace dtn */

#endif /* defined LIBODBC_ENABLED */

#endif  /* _BUNDle_DETAIL_H_ */
