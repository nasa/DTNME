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

#ifndef _TEMP_BUNDLE_H_
#define _TEMP_BUNDLE_H_

#include "Bundle.h"

namespace dtn {

/**
 * Class to represent a temporary bundle -- i.e. one that exists only
 * in memory and never goes to the persistent store.
 *
 * This is intended to be used for AnnounceBundles and other cases in
 * which the bundle is only needed to be sent over the wire and is
 * then immediately destroyed.
 */
class TempBundle : public Bundle {
public:
    /**
     * Constructor which forces the payload location to memory.
     */
    TempBundle() : Bundle(BundlePayload::MEMORY) {}    
};

} // namespace DTN


#endif /* _TEMP_BUNDLE_H_ */
