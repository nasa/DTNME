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

#ifndef _APIBUNDLEQUEUE_H_
#define _APIBUNDLEQUEUE_H_

#include <vector>
#include "dtn_types.h"
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/ScratchBuffer.h>

namespace dtn {

/**
 * Small encapsulating structure for API bundles.
 */
struct APIBundle {
    dtn_bundle_spec_t 			spec_;
    oasys::ScratchBuffer<char*,512>	payload_;
};

/**
 * Type definition of a simple vector of APIBundles.
 */
typedef std::vector<APIBundle*> APIBundleVector;

/**
 * Type definition of an oasys blocking message queue that stores
 * APIBundle structures.
 */
typedef oasys::MsgQueue<APIBundle*> APIBundleQueue;

} // namespace dtn

#endif /* _APIBUNDLEQUEUE_H_ */
