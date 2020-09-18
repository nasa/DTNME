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

#ifndef _CIPHERSUITE_PI10_H_
#define _CIPHERSUITE_PI10_H_

#ifdef BSP_ENABLED

#include <oasys/util/ScratchBuffer.h>
#include "bundling/BlockProcessor.h"
#include "PI_BlockProcessor.h"
#include "Ciphersuite_PI.h"

namespace dtn {

/**
 * Block processor implementation for the Payload Integrity Block
 * using the ECDSA algorithm.
 */
class Ciphersuite_PI10 : public Ciphersuite_PI {
public:
    typedef Ciphersuite::LocalBuffer LocalBuffer;

    const static int CSNUM_PI = 10;

    virtual u_int16_t cs_num() {
        return CSNUM_PI;
    };

    virtual u_int16_t hash_len() {
        return 384;
    }

private:
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_PI10_H_ */
