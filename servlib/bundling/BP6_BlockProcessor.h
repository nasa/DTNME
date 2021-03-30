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

#ifndef _BP6_BLOCKPROCESSOR_H_
#define _BP6_BLOCKPROCESSOR_H_

#include <third_party/oasys/compat/inttypes.h>

#include "BundleProtocolVersion6.h"
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "BP_Local.h"

namespace dtn {

class Bundle;
class Link;
class OpaqueContext;

#define BP_SUCCESS (int)(0)
#define BP_FAIL    (int)(-1)

/**
 * Base class for the protocol handling of bundle blocks, including
 * the core primary and payload handling, security, and other
 * extension blocks.
 */
class BP6_BlockProcessor : public BlockProcessor {
public:
    
    /**
     * Constructor that takes the block typecode. Generally, typecodes
     * should be defined in BundleProtocolVersion6::bundle_block_type_t, but
     * the field is defined as an int so that handlers for
     * non-specified blocks can be defined.
     */
    BP6_BlockProcessor(int block_type);
                   
    /**
     * Virtual destructor.
     */
    virtual ~BP6_BlockProcessor();
    
    /**
     * First callback for parsing blocks that is expected to append a
     * chunk of the given data to the given block. When the block is
     * completely received, this should also parse the block into any
     * fields in the bundle class.
     *
     * The base class implementation parses the block preamble fields
     * to find the length of the block and copies the preamble and the
     * data in the block's contents buffer.
     *
     * This and all derived implementations must be able to handle a
     * block that is received in chunks, including cases where the
     * preamble is split into multiple chunks.
     *
     * @return the amount of data consumed or -1 on error
     */
    virtual ssize_t consume(Bundle*    bundle,
                        SPtr_BlockInfo& block,
                        u_char*    buf,
                        size_t     len) override;

    /**
     * Perform any needed action in the case where a block/bundle
     * has been reloaded from store
     */
    virtual int reload_post_process(Bundle*       bundle,
                                    SPtr_BlockInfoVec& block_list,
                                    SPtr_BlockInfo&    block) override;


    /**
     * Validate the block. This is called after all blocks in the
     * bundle have been fully received.
     *
     * @return true if the block passes validation
     */
    virtual bool validate(const Bundle*           bundle,
                          SPtr_BlockInfoVec&           block_list,
                          SPtr_BlockInfo&              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason) override;


    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual int prepare(const Bundle*    bundle,
                        SPtr_BlockInfoVec&    xmit_blocks,
                        const SPtr_BlockInfo&  source,
                        const LinkRef&   link,
                        list_owner_t     list) override;
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.  It MUST add any EID references it needs by
     * calling block->add_eid(), then call generate_preamble(), which
     * will add the EIDs to the primary block's dictionary and write
     * their offsets to this block's preamble.
     */
    virtual int generate(const Bundle*  bundle,
                         SPtr_BlockInfoVec&  xmit_blocks,
                         SPtr_BlockInfo&     block,
                         const LinkRef& link,
                         bool           last) = 0;
    
    /**
     * Third callback for transmitting a bundle. This pass should
     * generate any data (such as security signatures) for the block
     * that may depend on other blocks' contents.
     *
     * The base class implementation does nothing. 
     * 
     * We pass xmit_blocks explicitly to indicate that ALL blocks
     * might be changed by finalize, typically by being encrypted.
     * Parameters such as length might also change due to padding
     * and encapsulation.
     */
    virtual int finalize(const Bundle*  bundle, 
                         SPtr_BlockInfoVec&  xmit_blocks, 
                         SPtr_BlockInfo&     block, 
                         const LinkRef& link) override;

    /**
     * Accessor to virtualize read-only processing contents of the
     * block in various ways. This is overloaded by the payload since
     * the contents are not actually stored in the BlockInfo contents_
     * buffer but rather are on-disk.
     *
     * Processing can be anything the calling routine wishes, such as
     * digest of the block, encryption, decryption etc. This routine
     * is permitted to process the data in several calls to the target
     * "func" routine as long as the data is processed in order and
     * exactly once.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void process(process_func*    func,
                         const Bundle*    bundle,
                         const SPtr_BlockInfo& caller_block,
                         const SPtr_BlockInfo& target_block,
                         size_t           offset,            
                         size_t           len,
                         OpaqueContext*   context) override;
    
    /**
     * Similar to process() but for potentially mutating processing
     * functions.
     *
     * The function returns true iff it modified the target_block.
     */
    virtual bool mutate(mutate_func*     func,
                        Bundle*          bundle,
                        const SPtr_BlockInfo& caller_block,
                        SPtr_BlockInfo&       target_block,
                        size_t           offset,
                        size_t           len,
                        OpaqueContext*   context);

    /**
     * Accessor to virtualize copying contents out from the block
     * info. This is overloaded by the payload since the contents are
     * not actually stored in the BlockInfo contents_ buffer but
     * rather are on-disk.
     *
     * The base class implementation simply does a memcpy from the
     * contents into the supplied buffer.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void produce(const Bundle*    bundle,
                         const SPtr_BlockInfo& block,
                         u_char*          buf,
                         size_t           offset,
                         size_t           len) override;

    /**
     * General hook to set up a block with the given contents. Used
     * for creating generic extension blocks coming from the API.
     * Can be specialized for specific block types.
     * Also used when testing bundle protocol routines.
     * If the block has a list of EIDs, the list should be attached
     * to the block before calling this routine. The EIDs will be
     * incorporated in the dictionary in the block_list object.
     */
    virtual void init_block(SPtr_BlockInfo&    block,
                            SPtr_BlockInfoVec& block_list,
                            Bundle*		  bundle,
                            u_int8_t      type,
                            u_int8_t      flags,
                            const u_char* bp,
                            size_t        len) override;
    
    /**
     * Return a one-line representation of the block.
     */
    virtual int format(oasys::StringBuffer* buf, BlockInfo *b = NULL) override;

protected:
    friend class BundleProtocolVersion6;
    friend class BlockInfo;
    friend class Ciphersuite;
    
    /**
     * Consume a block preamble consisting of type, flags(SDNV),
     * EID-list (composite field of SDNVs) and length(SDNV).
     * This method does not apply to the primary block, but is
     * suitable for payload and all extensions.
     */
    virtual ssize_t consume_preamble(SPtr_BlockInfoVec& recv_blocks,
                         SPtr_BlockInfo&    block,
                         u_char*       buf,
                         size_t        len,
                         u_int64_t*    flagp = NULL) override;
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.
     */
    virtual void generate_preamble(SPtr_BlockInfoVec& xmit_blocks,
                           SPtr_BlockInfo&    block,
                           u_int8_t      type,
                           u_int64_t     flags,
                           u_int64_t     data_length) override;
    
};

} // namespace dtn

#endif /* _BP6_BLOCKPROCESSOR_H_ */
