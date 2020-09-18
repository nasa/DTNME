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

#ifndef _BUNDLETIMESTAMP_H_
#define _BUNDLETIMESTAMP_H_

#include <sys/types.h>

namespace dtn {

/**
 * Simple struct definition for bundle creation timestamps. Although
 * quite similar to a struct timeval, the bundle protocol
 * specification mandates that timestamps should be calculated as
 * seconds since Jan 1, 2000 (not 1970) so we use a different type.
 */
struct BundleTimestamp {
    u_int64_t seconds_; ///< Seconds since 1/1/2000
    u_int64_t seqno_;   ///< Sequence number

    /**
     * Default constructor
     */
    BundleTimestamp()
        : seconds_(0), seqno_(0) {}
    
    /**
     * Constructor by parts.
     */
    BundleTimestamp(u_int64_t seconds, u_int64_t seqno)
        : seconds_(seconds), seqno_(seqno) {}
    
    /**
     * Return the current time in the correct format for the bundle
     * protocol, i.e. seconds since Jan 1, 2000 in UTC.
     */
    static u_int32_t get_current_time();

    /**
     * Check that the local clock setting is valid (i.e. is at least
     * up to date with the protocol.
     */
    static bool check_local_clock();

    /**
     * Operator overload for use in STL data structures.
     */
    bool operator==(const BundleTimestamp& other) const
    {
        return seconds_ == other.seconds_ &&
            seqno_ == other.seqno_;
    }
        
    /**
     * Operator overload for use in STL data structures.
     */
    bool operator<(const BundleTimestamp& other) const
    {
        if (seconds_ < other.seconds_) return true;
        if (seconds_ > other.seconds_) return false;
        return (seqno_ < other.seqno_);
    }
        
    /**
     * Operator overload for use in STL data structures.
     */
    bool operator>(const BundleTimestamp& other) const
    {
        if (seconds_ > other.seconds_) return true;
        if (seconds_ < other.seconds_) return false;
        return (seqno_ > other.seqno_);
    }
        
    /**
     * The number of seconds between 1/1/1970 and 1/1/2000.
     */
    static u_int32_t TIMEVAL_CONVERSION;
};

} // namespace dtn

#endif /* _BUNDLETIMESTAMP_H_ */
