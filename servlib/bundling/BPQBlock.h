/*
 *    Copyright 2010-2012 Trinity College Dublin
 *    Copyright 2010-2012 Folly Consulting Ltd
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

#ifndef _BPQ_BLOCK_H_
#define _BPQ_BLOCK_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include "BlockInfo.h"
#include "BPQFragmentList.h"
#include <oasys/debug/Log.h>
namespace dtn {

/**
 * This class is the decoded version of the data block that comes with a
 * BP Query Extension Block.  For convenience a BPQBlock instance is created
 * whenever a Query Extension block is added to the recv_blocks or api_blocks
 * vector of a bundle and remains in existence until thr bundle  is deleted.
 * The BPQBlock is a derived class of BP_Local which in turn derives from
 * oasys::RefCountedObject.  A pointer to the BPQBlock can therefore be stored
 * in the locals_ data item of the BlockInfo instance that represents the
 * Query Extension Block in recv_blocks or api_blocks.  The BPQBlockProcessor
 * and other classes handling explicitly Query Extension Blocks can set/retrieve
 * this pointer via the set_locals() and locals() methods of the BlockInfo and
 * apply a dynamic_cast to 'downcast' the BP_Local pointer into a BPQBlock pointer.
 * Because it is a RefCountedObject it is automatically deleted when the BlockInfo
 * instance is deleted.  It can also be held in memory by incrementing its reference
 * count (add_ref()/del_ref() methods) but this mostly not necessary as it is
 * used only within the context of the BUndle instance where it is stored and
 * pointers to the structure are used only within a single routine.  Likewise
 * it is never modified after the creation of the bundle so locks are not needed.
 *
 * BPQBlocks are created when:
 * - The dtn_send API function is called with a BPQ extension block
 * - A bundle is received form a link containing a BPQ extension block
 * - A bundle is read back from persistent storage on bundle daemon startup
 * - A response bundle is created as a result of receiving a BPQ QUERY
 *
 * It is not necessary to duplicate the BPQBlock into BlockInfo instances
 * placed into xmit_blocks vectors created during bundle transmission as all the
 * routines processing these blocks treat them as opaque data.
 */

class BPQBlock : public BP_Local, public oasys::Logger
{
public:
    BPQBlock(const Bundle* bundle);
    BPQBlock(const u_char* buf, u_int buf_len);
    virtual ~BPQBlock();

    static bool validate(u_char* buf, size_t len);

    int write_to_buffer(u_char* buf, size_t len);

    typedef enum {
        KIND_QUERY          			= 0x00,
        KIND_RESPONSE       			= 0x01,
        KIND_RESPONSE_DO_NOT_CACHE_FRAG	= 0x02,
        KIND_PUBLISH					= 0x03,
    } kind_t;

    /// @{ Accessors
    kind_t          		kind()          const { return kind_; }
    u_int           		matching_rule() const { return matching_rule_; }
    const BundleTimestamp& 	creation_ts()  	const { return creation_ts_; }
    const EndpointID& 		source()        const { return source_; }
    u_int           		query_len()     const { return query_len_; }
    u_char*        		 	query_val()     const { return query_val_; }
    u_int           		length()        const;
    u_int					frag_len()		const { return fragments_.size(); }
    const BPQFragmentList& 	fragments()  	const { return fragments_; }
    bool					validated()		const { return validated_; }
    /// @}

    bool match				(const BPQBlock* other)  const;

    /// @{ Mutating accessors
    void        set_creation_ts(const BundleTimestamp &ts) { creation_ts_ = ts; }
    void        set_source(const EndpointID& eid) { source_ =eid; }
    /// @}
    /**
     * Add the new fragment in sorted order
     */
    void add_fragment (BPQFragment* fragment);

private:
    int initialise_from_block(BlockInfo* block, bool created_locally, const Bundle* bundle);       ///< Wrapper function called by constructor
    int initialise_from_buf(const u_char* buf, u_int buf_length);

    void log_block_info(BlockInfo* block);

    int extract_kind			(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_matching_rule	(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_creation_ts		(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_source			(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_query			(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_fragments		(const u_char* buf, u_int* buf_index, u_int buf_length);

    kind_t kind_;               	///< Query || Response
    u_int matching_rule_;       	///< Exact
    BundleTimestamp creation_ts_;	///< Original Creation Timestamp
    EndpointID source_;				///< Original Source EID
    u_int query_len_;           	///< Length of the query value
    u_char* query_val_;         	///< Query value
    BPQFragmentList fragments_;  	///< List of fragments returned
    bool validated_;				///< True if initialization worked as expected
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* _BPQ_BLOCK_H_ */

