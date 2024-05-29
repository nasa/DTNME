#ifndef _PAYLOAD_BLOCK_PROCESSOR_HELPER_H_
#define _PAYLOAD_BLOCK_PROCESSOR_HELPER_H_

#include <third_party/oasys/util/StreamBuffer.h>

#include "BlockInfo.h"

namespace dtn {

/**
 * This is a BP_Local object used by the PayloadBlockProcessors to maintain 
 * a wroking buffer that allows for reading in chunks of data larger than the
 * production buffer allows.
 * 
 */
class PayloadBlockProcessorHelper : public BP_Local {
public:

    /// Constructor
    PayloadBlockProcessorHelper(size_t alloc_len);

    /// Destructor
    ~PayloadBlockProcessorHelper();

    /// Working buffer for reading in large-ish blocks of a payload
    /// before chunking it into the production buffer
    oasys::StreamBuffer work_buf_;
};

/**
 * Typedef for a reference to a PayloadBlockProcessorHelper.
 */
//typedef oasys::Ref<PayloadBlockProcessorHelper> PayloadBlockProcessorHelperRef;

} // namespace dtn

#endif /* _PAYLOAD_BLOCK_PROCESSOR_HELPER_H_ */
