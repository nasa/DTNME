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

#ifndef _CIPHERSUITE_BA9_H_
#define _CIPHERSUITE_BA9_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "BA_BlockProcessor.h"
#include "Ciphersuite_BA.h"

namespace dtn {

/**
 * Block processor implementation for the Bundle Authentication Block
 */
class Ciphersuite_BA9 : public Ciphersuite_BA {
public:
    const static int CSNUM_BA = 9;
    virtual u_int16_t cs_num() {
        return CSNUM_BA;
    };

    const static int RESULT_LEN = 48;
    virtual size_t result_len() {
    	return RESULT_LEN;
    }

private:
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_BA9_H_ */
