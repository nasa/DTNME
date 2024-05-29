/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifndef _IPN_ECHO_REGISTRATION_H_
#define _IPN_ECHO_REGISTRATION_H_

#include "Registration.h"

namespace dtn {

/**
 * Internal registration for the dtnping application.
*/
class IpnEchoRegistration : public Registration {

public:
    IpnEchoRegistration(const SPtr_EID& sptr_eid);
    int deliver_bundle(Bundle* bundle, SPtr_Registration& sptr_reg) override;
};

} // namespace dtn

#endif /* _IPN_ECHO_REGISTRATION_H_ */
