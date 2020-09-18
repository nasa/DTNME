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

#ifndef _EXPIRATION_TIMER_H_
#define _EXPIRATION_TIMER_H_

#include <oasys/thread/Timer.h>
#include "BundleRef.h"

namespace dtn {

/**
 * Bundle expiration timer class.
 *
 * The timer is started when the bundle first arrives at the daemon,
 * and is cancelled when the daemon removes it from the pending list.
 *
 */
class ExpirationTimer : public oasys::Timer {
public:
    ExpirationTimer(Bundle* bundle);

    virtual ~ExpirationTimer() {} 

    /// The reference to the bundle, which is public since 
    BundleRef bundleref_;
    
protected:
    void timeout(const struct timeval& now);

};

} // namespace dtn

#endif /* _EXPIRATION_TIMER_H_ */
