/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _BP_LOCAL_H_
#define _BP_LOCAL_H_

#include <oasys/util/Ref.h>
#include <oasys/util/RefCountedObject.h>
#include <oasys/debug/Log.h>


namespace dtn {

/**
 * Helper class for BlockProcessor local state for a block.
 */
class BP_Local : public oasys::RefCountedObject {
public:
    /**
     * Constructor.
     */
    BP_Local() : RefCountedObject("/dtn/bp_local/refs") {
    	log_debug_p("/dtn/bp_local/refs", "constructor");
    };

    /**
     * Virtual destructor.
     */
    virtual ~BP_Local() { log_debug_p("/dtn/bp_local/refs", "destructor"); };

    virtual void dummy() {};

};  /* BP_Local */

/**
 * Typedef for a reference to a BP_Local.
 */
typedef oasys::Ref<BP_Local> BP_LocalRef;

} // namespace dtn

#endif /* _BP_LOCAL_H_ */
