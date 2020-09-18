/*
 *    Copyright 2005-2006 Intel Corporation
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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _SIM_REGISTRATION_H_
#define _SIM_REGISTRATION_H_

#include <map>

#include "Node.h"
#include "reg/Registration.h"

using namespace dtn;

namespace dtnsim {

/**
 * Registration used for the simulator
 */
//dz debug class SimRegistration : public oasys::Formatter, public APIRegistration {
class SimRegistration : public APIRegistration {
public:
    SimRegistration(Node* node, const EndpointID& endpoint, u_int32_t expiration = 10);

    /**
     * Deliver the given bundle.
     */
    void deliver_bundle(Bundle* bundle);

    /**
     * Virtual from Formatter
     */
    int format(char* buf, size_t sz) const;

protected:
    Node* node_;
};

} // namespace dtnsim

#endif /* _SIM_REGISTRATION_H_ */
