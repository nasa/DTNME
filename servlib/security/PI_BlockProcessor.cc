/*
 *    Copyright 2006 SPARTA Inc
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "PI_BlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
PI_BlockProcessor::PI_BlockProcessor()
    : AS_BlockProcessor(BundleProtocol::PAYLOAD_SECURITY_BLOCK)
{
}

int PI_BlockProcessor::format(oasys::StringBuffer* buf, BlockInfo *block) {
    buf->append("PIB ");
    return AS_BlockProcessor::format(buf, block);
}


} // namespace dtn

#endif /* BSP_ENABLED */
