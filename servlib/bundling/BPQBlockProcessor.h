/*
 *    Copyright 2010-2012 Trinity College Dublin
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

#ifndef _BPQ_BLOCK_PROCESSOR_H_
#define _BPQ_BLOCK_PROCESSOR_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include "BlockProcessor.h"

#include "BundleProtocol.h"
#include "BlockInfo.h"
#include "BPQBlock.h"
#include "Bundle.h"

#include <oasys/util/StringBuffer.h>
#include <oasys/util/Singleton.h>

namespace dtn {


/**
 * Block processor implementation for the BPQ Extension Block
 */
class BPQBlockProcessor : public BlockProcessor,
                          public oasys::Singleton<BPQBlockProcessor> {
public:
    /// Constructor
    BPQBlockProcessor();

    /// @{ Virtual from BlockProcessor
    int consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len);

    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const BlockInfo* source,
                const LinkRef&   link,
                list_owner_t     list);

    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last);

    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason);

    /**
     * Overrides reload_post_process in base class.
     * Sets up BP_Local data as a BPQBlock from the data read into the block.
     */
    virtual int reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block);

    /**
     * Overrides init_block in base class.
     * Sets up BP_Local data as a BPQBlock from the supplied data in the (bp,len) pair.
     */
    virtual void init_block(BlockInfo*    block,
                            BlockInfoVec* block_list,
                            Bundle*		  bundle,
                            u_int8_t      type,
                            u_int8_t      flags,
                            const u_char* bp,
                            size_t        len);

    int format(oasys::StringBuffer* buf, BlockInfo *block);

    /// @}

//private:
//    BPQBlock* create_block(const Bundle* const bundle) const;
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* _BPQ_BLOCK_PROCESSOR_H_ */
