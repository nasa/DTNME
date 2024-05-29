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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "TokenBucketLeaky.h"

namespace oasys {

//----------------------------------------------------------------------
TokenBucketLeaky::TokenBucketLeaky(const char* logpath,
                                   u_int64_t   depth,   /* in bits */
                                   u_int64_t   rate     /* in seconds */)
    : TokenBucket(logpath,depth,rate)
{
    // overrides
    tokens_ = 0;

    log_debug("initialized token bucket with depth %llu and rate %llu",
              U64FMT(depth_), U64FMT(rate_));
    last_update_.get_time();
}
//----------------------------------------------------------------------
void
TokenBucketLeaky::update()
{
    Time now;
    now.get_time();

    // time sometimes jumps backwards!!
    int ctr = 0;
    while (now < last_update_) {
        now.get_time();
        ++ctr;
    }

    if (tokens_ == (int64_t)0) {
        log_debug("update: bucket already empty, nothing to update");
        last_update_ = now;
        return;
    }

    u_int32_t elapsed = (now - last_update_).in_microseconds();
    int64_t new_tokens = (rate_ * elapsed) / 1000000;

    if (new_tokens > 0) {
        if ((int64_t) new_tokens > tokens_) { // will tokens go negative?
            new_tokens = tokens_;
        }

        log_debug("update: leaking %lld/%lld spent tokens after %u microseconds",
                  I64FMT(-new_tokens), I64FMT(tokens_), elapsed);
        tokens_ -= new_tokens;
        last_update_ = now;
        
    } else {
        // there's a chance that, for a slow rate, that the elapsed
        // time isn't enough to fill even a single token. in this
        // case, we leave last_update_ to where it was before,
        // otherwise we might starve the bucket.
        log_debug("update: %u milliseconds elapsed not enough to fill any tokens",
                  elapsed);
    }
}

//----------------------------------------------------------------------
bool
TokenBucketLeaky::drain(u_int64_t length, bool only_if_enough) // should be fill since we are leaky
{
    update();

    bool enough = (tokens_ > 0) ? false : true;     // ((u_int64_t)tokens_ == 0);

    log_debug("drain: filling %llu/%lld tokens into bucket",
              U64FMT(length), I64FMT(tokens_));

    if (enough || !only_if_enough) {
        tokens_ += length;
    }

    return enough;
}

//----------------------------------------------------------------------
bool
TokenBucketLeaky::try_to_drain(u_int64_t length)
{
    return drain(length, true);
}

//----------------------------------------------------------------------
Time
TokenBucketLeaky::time_to_level(int64_t n)
{
    update();

    int64_t need = 0;
    if (n > tokens_) {
        need = n - tokens_;
    }

    Time t(need / rate_,
           ((need * 1000000) / rate_) % 1000000);
    
    log_debug("time_to_level(%lld): "
              "%lld more tokens will arrive in %u.%u "
              "(tokens %lld rate %llu)",
              U64FMT(n), U64FMT(need), t.sec_, t.usec_,
              I64FMT(tokens_), U64FMT(rate_));
    return t;
}

//----------------------------------------------------------------------
Time
TokenBucketLeaky::time_to_fill()  // should be drain since we are leaky
{
    return time_to_level(0);
}

//----------------------------------------------------------------------
void
TokenBucketLeaky::empty()
{
    tokens_      = 0;
    last_update_.get_time();

    log_debug("empty: clearing bucket");
}

} // namespace oasys
