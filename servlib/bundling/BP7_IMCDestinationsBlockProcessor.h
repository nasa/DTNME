/*
 *    Copyright 2022 United States Government as represented by NASA
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

#ifndef _BP7_IMC_DESTINATIONS_BLOCK_PROCESSOR_H_
#define _BP7_IMC_DESTINATIONS_BLOCK_PROCESSOR_H_

#include <third_party/oasys/util/Singleton.h>

#include "BP7_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP7_IMCDestinationsBlockProcessor : public BP7_BlockProcessor {
public:
    /// Constructor
    BP7_IMCDestinationsBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    ssize_t consume(Bundle*    bundle,
                BlockInfo* block,
                u_char*    buf,
                size_t     len) override;
    
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
    
    /// @}
    //

protected:

    int64_t encode_block_data(uint8_t* buf, uint64_t buflen,
                              std::string& link_name, bool is_home_region,
                              const Bundle* bundle, int64_t& encoded_len);

    int64_t decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle);

};

} // namespace dtn

#endif /* _BP7_IMC_DESTINATIONS_BLOCK_PROCESSOR_H_ */
