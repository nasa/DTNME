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

#ifndef _LOGGING_REGISTRATION_H_
#define _LOGGING_REGISTRATION_H_

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/Thread.h>

#include "Registration.h"

namespace dtn {


/**
 * A simple utility class used mostly for testing registrations.
 *
 * When created, this sets up a new registration within the daemon,
 * and for any bundles that arrive, outputs logs of the bundle header
 * fields as well as the payload data (if ascii). The implementation
 * is structured as a thread that blocks (forever) waiting for bundles
 * to arrive on the registration's bundle list, then logging the
 * bundles and looping again.
 */
class LoggingRegistration : public Registration {
public:
    LoggingRegistration(const SPtr_EIDPattern& sptr_endpoint);
    int deliver_bundle(Bundle* bundle, SPtr_Registration& sptr_reg) override;
};

} // namespace dtn

#endif /* _LOGGING_REGISTRATION_H_ */
