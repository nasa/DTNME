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

    u_int64_t ipn_echo_max_length_;

public:
    IpnEchoRegistration(const EndpointID& eid, u_int64_t ipn_echo_max_length);
    void deliver_bundle(Bundle* bundle);
};

} // namespace dtn

#endif /* _IPN_ECHO_REGISTRATION_H_ */
