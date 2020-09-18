/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifndef _METADATA_BLOCK_PROCESSOR_H_
#define _METADATA_BLOCK_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

class MetadataBlock;

/**
 * Block processor implementation for the metadata extension block.
 */
class MetadataBlockProcessor : public BlockProcessor {
public:

    /// Constructor
    MetadataBlockProcessor();

    /// @{ Virtual from BlockProcessor
    int64_t  consume(Bundle*    bundle,
                 BlockInfo* block,
                 u_char*    buf,
                 size_t     len);

    int reload_post_process(Bundle*       bundle,
                            BlockInfoVec* block_list,
                            BlockInfo*    block);

    bool validate(const Bundle*           bundle,
                  BlockInfoVec*           block_list,
                  BlockInfo*              block,
                  status_report_reason_t* reception_reason,
                  status_report_reason_t* deletion_reason);

    int prepare(const Bundle*     bundle,
                 BlockInfoVec*    xmit_blocks,
                 const BlockInfo* source,
                 const LinkRef&   link,
                 list_owner_t     list);

    int generate(const Bundle*  bundle,
                  BlockInfoVec*  xmit_blocks,
                  BlockInfo*     block,
                  const LinkRef& link,
                  bool           last);

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL);
    /// @}

    /**
     * Determines the generated metadata (not received metadata, which is
     * handled by the prepare() method) to be included in an outgoing bundle.
     */
    void prepare_generated_metadata(Bundle*        bundle,
                                    BlockInfoVec*  blocks,
                                    const LinkRef& link);

    /**
     * Deletes bundle state maintained for generated metadata.
     */
    void delete_generated_metadata(Bundle* bundle, const LinkRef& link);

private:

    /**
     * Parses a metadata extension block.
     *
     * @returns true if metadata parsed successfully, otherwise false.
     */
    bool parse_metadata(Bundle* bundle, BlockInfo* block);

    /**
     * Handles a metadata extension block processing error based on the
     * block preamble flags.
     *
     * @return true if the bundle in which the invalid block was received
     * should be deleted; otherwise false.
     */
    bool handle_error(const BlockInfo*        block,
                      status_report_reason_t* reception_reason,
                      status_report_reason_t* deletion_reason);
};

} // namespace dtn

#endif /* _METADATA_BLOCK_PROCESSOR_H_ */
