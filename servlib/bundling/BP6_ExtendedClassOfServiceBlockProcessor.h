/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifndef _BP6_EXTENDED_CLASS_OF_SERVICE_BLOCK_PROCESSOR_H_
#define _BP6_EXTENDED_CLASS_OF_SERVICE_BLOCK_PROCESSOR_H_

#ifdef ECOS_ENABLED

#include "BP6_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the Extended Class Of Service Block
 */
class BP6_ExtendedClassOfServiceBlockProcessor : public BP6_BlockProcessor {
public:
    typedef enum {
        ECOS_CRITICAL_BIT              = 0x01,
        ECOS_STREAMING_BIT             = 0x02,
        ECOS_FLOW_LABEL_BIT            = 0x04,
    } ecos_bit_flags_t;

    /// Constructor
    BP6_ExtendedClassOfServiceBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int prepare(const Bundle*    bundle,
                BlockInfoVec*    xmit_blocks,
                const SPtr_BlockInfo source,
                const LinkRef&   link,
                list_owner_t     list) override;
    
    int generate(const Bundle*  bundle,
                 BlockInfoVec*  xmit_blocks,
                 BlockInfo*     block,
                 const LinkRef& link,
                 bool           last) override;
    
    ssize_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len) override;

    /// @}
};

} // namespace dtn

#endif // ECOS_ENABLED

#endif //_BP6_EXTENDED_CLASS_OF_SERVICE_BLOCK_PROCESSOR_H_
