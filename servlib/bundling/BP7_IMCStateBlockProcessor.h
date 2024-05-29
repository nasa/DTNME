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

#ifndef _BP7_IMC_STATE_BLOCK_PROCESSOR_H_
#define _BP7_IMC_STATE_BLOCK_PROCESSOR_H_

#include <third_party/oasys/util/Singleton.h>

#include "BP7_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class BP7_IMCStateBlockProcessor : public BP7_BlockProcessor {
public:
    /// Constructor
    BP7_IMCStateBlockProcessor();
    
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

    /**
     * Initial CBOR block data encoder detremines which format to use and calls 
     * the approprite method.
     *     format version 0: used on "regular" IMC bundles with a destination other than imc::0.0
     *     format version 1: used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
     *     format version 2: used on IMC Briefing bundles 
     *         (these bundles have an IPN administrative destination and setting the imc_briefing
     *          flag on the bundle triggers adding this block to the outgoing serialized bundle)
     */
    int64_t encode_block_data(uint8_t* buf, uint64_t buflen,
                              const Bundle* bundle, int64_t& encoded_len);

    /**
     * CBOR block data encoder for format version 0 which is used on "regular" IMC bundles with 
     * a destination other than imc::0.0
     */
    int64_t encode_block_data_format_0(uint8_t* buf, uint64_t buflen,
                              const Bundle* bundle, int64_t& encoded_len);

    /**
     * CBOR block data encoder for format version 1 which is used on bundles with an 
     * IMC destination of imc::0.0 (IMC Group Petitions)
     */
    int64_t encode_block_data_format_1(uint8_t* buf, uint64_t buflen,
                              const Bundle* bundle, int64_t& encoded_len);

    /**
     * CBOR block data encoder for format version 2 which is used on IMC Briefing bundles 
     * (these bundles have an IPN administrative destination and setting the imc_briefing
     *  flag on the bundle triggers adding this block to the outgoing serialized bundle)
     */
    int64_t encode_block_data_format_2(uint8_t* buf, uint64_t buflen,
                              const Bundle* bundle, int64_t& encoded_len);

    /**
     * Initial CBOR block data decoder detremines which format to use and calls 
     * the approprite method.
     *     format version 0: used on "regular" IMC bundles with a destination other than imc::0.0
     *     format version 1: used on bundles with an IMC destination of imc::0.0 (IMC Group Petitions)
     *     format version 2: used on IMC Briefing bundles 
     *         (these bundles have an IPN administrative destination and setting the imc_briefing
     *          flag on the bundle triggers adding this block to the outgoing serialized bundle)
     */
    int64_t decode_block_data(uint8_t* data_buf, size_t data_len, Bundle* bundle);

    /**
     * CBOR block data decoder for format version 0 which is used on "regular" IMC bundles with 
     * a destination other than imc::0.0
     */
    int64_t decode_block_data_format_0(CborValue& cvBlockArray, Bundle* bundle);

    /**
     * CBOR block data decoder for format version 1 which is used on bundles with an 
     * IMC destination of imc::0.0 (IMC Group Petitions)
     */
    int64_t decode_block_data_format_1(CborValue& cvBlockArray, Bundle* bundle);

    /**
     * CBOR block data decoder for format version 2 which is used on IMC Briefing bundles 
     * (these bundles have an IPN administrative destination and setting the imc_briefing
     *  flag on the bundle triggers adding this block to the outgoing serialized bundle)
     */
    int64_t decode_block_data_format_2(CborValue& cvBlockArray, Bundle* bundle);

};

} // namespace dtn

#endif /* _BP7_IMC_STATE_BLOCK_PROCESSOR_H_ */
