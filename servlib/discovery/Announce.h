/*
 *    Copyright 2006 Baylor University
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

#ifndef _ANNOUNCE_H_
#define _ANNOUNCE_H_

#include <oasys/debug/Log.h>
#include <oasys/thread/Timer.h>
#include <string>
#include "naming/EndpointID.h"
#include "conv_layers/ConvergenceLayer.h"

#ifndef FOUR_BYTE_ALIGN
#define FOUR_BYTE_ALIGN(x) (((x) % 4) != 0) ? ((x) + (4 - ((x) % 4))) : (x)
#endif

namespace dtn {

/**
 * Announce represents a ConvergenceLayer (Interface). Each announce
 * instance records its CL's address and the interval at which to advertise
 * to or poll for neighbors.  Discovery maintains a list of Announce
 * which serve as the basis for its advertisement.
 *
 * Additionally, Announce serves as a responder.  For each discovery
 * it creates a new Contact to the remote node by placing the appropriate
 * call into its CL.
 */
class Announce : public oasys::Logger
{
public:

    /**
     * The name of this Announce instance
     */
    const std::string& name() { return name_; }

    /**
     * Which type of CL is represented by this Announce
     */
    const std::string& type() { return type_; }

    /**
     * Return a string representation of the ConvergenceLayer
     * address info to be advertised by parent Discovery
     */
    const std::string& local_addr() { return local_; }
    
    /**
     * Hook for derived classes to format information to be advertised
     */
    virtual size_t format_advertisement(u_char* buf, size_t len) = 0;

    virtual ~Announce() {}

    /**
     * Return the number of milliseconds remaining until the interval 
     * expires, or 0 if it's already expired
     */
    u_int interval_remaining()
    {
        struct timeval now;
        ::gettimeofday(&now,0);
        u_int timediff = TIMEVAL_DIFF_MSEC(now,data_sent_);
        if (timediff > interval_)
            return 0;

        return (interval_ - timediff);
    }

    /**
     * Factory method for creating instances of derived classes
     */
    static Announce* create_announce(const std::string& name,
                                     ConvergenceLayer* cl,
                                     int argc, const char* argv[]);

    /**
     * Number of milliseconds between announcements
     */
    u_int interval() { return interval_; }

protected:
    Announce(const char* logpath = "/dtn/discovery/announce")
        : oasys::Logger("Announce", "%s", logpath),
          cl_(NULL), interval_(0)
    {
        ::gettimeofday(&data_sent_,0);
    }

    virtual bool configure(const std::string& name,
                           ConvergenceLayer* cl,
                           int argc, const char* argv[]) = 0;

    ConvergenceLayer* cl_; ///< CL represented by this Announce
    std::string local_; ///< Beacon info to advertise
    std::string name_;  ///< name for this beacon instance
    std::string type_;  ///< pulled from cl_
    u_int interval_;    ///< interval (in milliseconds) for beacon header

    struct timeval data_sent_; ///< mark each time data is sent
private:
    Announce(const Announce&)
        : oasys::Logger("Announce","/dtn/discovery/beacon") {}
}; // Announce

} // dtn

#endif // _ANNOUNCE_H_
