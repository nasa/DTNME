
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/debug/Log.h>

#include "PayloadBlockProcessorHelper.h"

namespace dtn {

/// Constructor
PayloadBlockProcessorHelper::PayloadBlockProcessorHelper(size_t alloc_len)
    : work_buf_(alloc_len)

{
}


/// Destructor
PayloadBlockProcessorHelper::~PayloadBlockProcessorHelper()
{
}


} // namespace dtn
