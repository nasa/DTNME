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

#ifndef _OASYS_TOKEN_BUCKET_LEAKY_H_
#define _OASYS_TOKEN_BUCKET_LEAKY_H_

#include "Time.h"

#include "../debug/Logger.h"
#include "TokenBucket.h"

namespace oasys {

/**
 * A basic leaky bucket implementation.
 */
class TokenBucketLeaky : public TokenBucket {

public:
    /**
     * Constructor that takes the initial depth and rate parameters.
     */
    TokenBucketLeaky(const char* logpath,
                     u_int64_t   depth,
                     u_int64_t   rate     /* in tokens per second */);

    virtual  ~TokenBucketLeaky(void){ };
    /**
     * Drains the specified amount from the bucket. If only_if_enough
     * is set, this will only drain the amount if the bucket has
     * capacity, otherwise this may make the bucket contain a negative
     * number of tokens
     *
     * Note that this function will first try to fill() the bucket, so
     * in fact, this is the only function (besides the constructor)
     * that needs to be called in order for the bucket to work
     * properly.
     *
     * @return true if there were enough tokens in the bucket
     */
    virtual bool drain(u_int64_t length, bool only_if_enough = false);

    /**
     * Syntactic sugar for drain() with only_if_enough set to true.
     */
    virtual bool try_to_drain(u_int64_t length);

    /**
     * Update the number of tokens in the bucket without draining any.
     * Since drain() will also update the number of tokens, there's
     * usually no reason to call this function explicitly.
     */
    virtual void update();

    /**
     * Return the amount of time until the bucket will be full again.
     */
    virtual Time time_to_fill();

    /**
     * Return the amount of time (in millseconds) until the bucket
     * has at least n tokens in it.
     */
    virtual Time time_to_level(int64_t n);

    /**
     * Empty the bucket.
     */
    virtual void empty();
    
protected:
};

} // namespace oasys

#endif /* _OASYS_TOKEN_BUCKET_LEAKY_H_ */
