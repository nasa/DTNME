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

#ifndef _BLOCKPROCESSOR_H_
#define _BLOCKPROCESSOR_H_

#include <oasys/compat/inttypes.h>

#include "BundleProtocol.h"
#include "BlockInfo.h"
#include "BP_Local.h"

namespace dtn {

class BlockInfo;
class BlockInfoVec;
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
class BlockProcessor {
public:
    /**
     * Typedef for a process function pointer.
     */
    typedef void (process_func)(const Bundle*    bundle,
                                const BlockInfo* caller_block,
                                const BlockInfo* target_block,
                                const void*      buf,
                                size_t           len,
                                OpaqueContext*   r);
    /**
     * Typedef for a mutate function pointer.
     */
    typedef bool (mutate_func)(const Bundle*    bundle,
                               const BlockInfo* caller_block,
                               BlockInfo*       target_block,
                               void*            buf,
                               size_t           len,
                               OpaqueContext*   context);

    /// @{ Import some typedefs from other classes
    typedef BlockInfo::list_owner_t list_owner_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;
    /// @}
    
    /**
     * Constructor that takes the block typecode. Generally, typecodes
     * should be defined in BundleProtocol::bundle_block_type_t, but
     * the field is defined as an int so that handlers for
     * non-specified blocks can be defined.
     */
    BlockProcessor(int block_type);
                   
    /**
     * Virtual destructor.
     */
    virtual ~BlockProcessor();
    
    /// @{ Accessors
    int block_type() { return block_type_; }
    /// @}
    
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
    virtual int64_t consume(Bundle*    bundle,
                        BlockInfo* block,
                        u_char*    buf,
                        size_t     len);

    /**
     * Perform any needed action in the case where a block/bundle
     * has been reloaded from store
     */
    virtual int reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block);


    /**
     * Validate the block. This is called after all blocks in the
     * bundle have been fully received.
     *
     * @return true if the block passes validation
     */
    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason);


#ifdef BSP_ENABLED
    /**
     * Validate the security result for a BSP block. This is called 
     * after all blocks in the bundle have been fully received.
     *
     * @return true if the block passes security result validation
     */
    virtual bool validate_security_result(const Bundle*           bundle,
                                          const BlockInfoVec*     block_list,
                                          BlockInfo*              block);
#endif

    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list);
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.  It MUST add any EID references it needs by
     * calling block->add_eid(), then call generate_preamble(), which
     * will add the EIDs to the primary block's dictionary and write
     * their offsets to this block's preamble.
     */
    virtual int generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
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
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link);

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
                         const BlockInfo* caller_block,
                         const BlockInfo* target_block,
                         size_t           offset,            
                         size_t           len,
                         OpaqueContext*   context);
    
    /**
     * Similar to process() but for potentially mutating processing
     * functions.
     *
     * The function returns true iff it modified the target_block.
     */
    virtual bool mutate(mutate_func*     func,
                        Bundle*          bundle,
                        const BlockInfo* caller_block,
                        BlockInfo*       target_block,
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
                         const BlockInfo* block,
                         u_char*          buf,
                         size_t           offset,
                         size_t           len);

    /**
     * General hook to set up a block with the given contents. Used
     * for creating generic extension blocks coming from the API.
     * Can be specialized for specific block types.
     * Also used when testing bundle protocol routines.
     * If the block has a list of EIDs, the list should be attached
     * to the block before calling this routine. The EIDs will be
     * incorporated in the dictionary in the block_list object.
     */
    virtual void init_block(BlockInfo*    block,
                            BlockInfoVec* block_list,
                            Bundle*		  bundle,
                            u_int8_t      type,
                            u_int8_t      flags,
                            const u_char* bp,
                    size_t        len);
    
    /**
     * Return a one-line representation of the block.
     */
    virtual int format(oasys::StringBuffer* buf, BlockInfo *b = NULL);

protected:
    friend class BundleProtocol;
    friend class BlockInfo;
    friend class Ciphersuite;
    
    /**
     * Consume a block preamble consisting of type, flags(SDNV),
     * EID-list (composite field of SDNVs) and length(SDNV).
     * This method does not apply to the primary block, but is
     * suitable for payload and all extensions.
     */
    int consume_preamble(BlockInfoVec* recv_blocks,
                         BlockInfo*    block,
                         u_char*       buf,
                         size_t        len,
                         u_int64_t*    flagp = NULL);
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.
     */
    void generate_preamble(BlockInfoVec* xmit_blocks,
                           BlockInfo*    block,
                           u_int8_t      type,
                           u_int64_t     flags,
                           u_int64_t     data_length);
    
private:
    /// The block typecode for this handler
    int block_type_;
};

} // namespace dtn

#endif /* _BLOCKPROCESSOR_H_ */
