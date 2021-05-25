/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _ADMIN_REGISTRATION_H_
#define _ADMIN_REGISTRATION_H_

#include "Registration.h"

#include "bundling/BIBEExtractor.h"

namespace dtn {

/**
 * Internal registration that recieves all administrative bundles
 * destined for the router itself (i.e. status reports, custody
 * acknowledgements, ping bundles, etc.)
*/
class AdminRegistration : public Registration {
public:
    AdminRegistration();

    /**
     * Deliver the given bundle.
     */
    void deliver_bundle(Bundle* bundle);

protected:

    /**
     * Deliver the given bundle based on BP version
     */
    void deliver_bundle_bp6(Bundle* bundle);
    void deliver_bundle_bp7(Bundle* bundle);

    const char* fld_name() { return fld_name_.c_str(); }

private:
    std::string fld_name_;

};

} // namespace dtn

#endif /* _ADMIN_REGISTRATION_H_ */
