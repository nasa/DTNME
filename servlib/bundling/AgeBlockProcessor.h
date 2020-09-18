#ifndef _AGE_BLOCK_PROCESSOR_H_
#define _AGE_BLOCK_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the Age Extension Block
 */
class AgeBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    AgeBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
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
    
    int64_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len);

    int format(oasys::StringBuffer* buf, BlockInfo *b = NULL);

    /// @}
};

} // namespace dtn

#endif /* _AGE_BLOCK_PROCESSOR_H_ */
