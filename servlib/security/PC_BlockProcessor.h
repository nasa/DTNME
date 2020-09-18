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

#ifndef _PC_BLOCK_PROCESSOR_H_
#define _PC_BLOCK_PROCESSOR_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "Ciphersuite.h"
#include "AS_BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the Payload Confidentiality
 * Block
 */
class PC_BlockProcessor : public AS_BlockProcessor {
public:
    PC_BlockProcessor();

    virtual int format(oasys::StringBuffer* buf, BlockInfo *block);
    
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _PC_BLOCK_PROCESSOR_H_ */
