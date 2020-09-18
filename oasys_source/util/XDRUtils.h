 /*
 *    Copyright 2006 Intel Corporation
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

#ifndef _XDRUTILS_H_
#define _XDRUTILS_H_

#include "../compat/rpc.h"

namespace oasys {

/**
 * Similar idea to ScopePointer and ScopeMalloc (see Pointers.h) but
 * meant to ensure that xdr_free() is called before a fn returns.
 */
class ScopeXDRFree {
public:
    ScopeXDRFree(xdrproc_t proc = NULL, char* ptr = NULL)
        : proc_(proc), ptr_(ptr) {}
    
    ~ScopeXDRFree() {
        if (ptr_) {
            xdr_free(proc_, ptr_);
            proc_ = 0;
            ptr_  = 0;
        }
    }

    /**
     * Not implemented on purpose. Don't handle assignment to another
     * ScopeXDRFree
     */
    ScopeXDRFree& operator=(const ScopeXDRFree&); 
    
private:
    xdrproc_t proc_;
    char*     ptr_;
};

} // namespace oasys

#endif /* _XDRUTILS_H_ */
