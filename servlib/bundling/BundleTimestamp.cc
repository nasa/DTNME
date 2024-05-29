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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <time.h>
#include <third_party/oasys/debug/Log.h>

#include "BundleTimestamp.h"

namespace dtn {

/**
 * The number of seconds between 1/1/1970 and 1/1/2000.
 */
size_t BundleTimestamp::TIMEVAL_CONVERSION_SECS = 946684800;

size_t
BundleTimestamp::get_current_time_millis()
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    
    ASSERT((size_t)now.tv_sec >= TIMEVAL_CONVERSION_SECS);
    return ((now.tv_sec - TIMEVAL_CONVERSION_SECS) * 1000) + (now.tv_usec / 1000);
}

size_t
BundleTimestamp::get_current_time_secs()
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    
    ASSERT((size_t)now.tv_sec >= TIMEVAL_CONVERSION_SECS);
    return now.tv_sec - TIMEVAL_CONVERSION_SECS;
}

bool
BundleTimestamp::check_local_clock()
{
    struct timeval now;
    ::gettimeofday(&now, 0);

    if ((size_t)now.tv_sec < TIMEVAL_CONVERSION_SECS) {
        logf("/dtn/bundle/timestamp", oasys::LOG_ERR,
             "invalid local clock setting: "
             "current time '%s' is before Jan 1, 2000",
             ctime((const time_t*)&now.tv_sec));

        log_err_p("/dtn/bundle/timestamp",
                          "invalid local clock setting: "
                          "current time '%s' is before Jan 1, 2000",
                          ctime((const time_t*)&now.tv_sec));
        return false;
    }

    return true;
}
} // namespace dtn
