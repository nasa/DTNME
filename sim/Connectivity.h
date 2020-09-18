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

#ifndef _CONNECTIVITY_H_
#define _CONNECTIVITY_H_

#include "SimEventHandler.h"
#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Logger.h>
#include <oasys/util/StringUtils.h>

namespace dtnsim {

class Node;


/**
 * Helper struct to store the current connectivity settings
 * between a pair (or set) of nodes.
 */
struct ConnState {
    /**
     * Default constructor, also implicitly the default connectivity
     * state.
     */
    ConnState(): open_(true), bw_(100000), latency_(0.01) {}

    /**
     * Constructor with explicit settings.
     */
    ConnState(bool open, int bw, int latency)
        : open_(open), bw_(bw), latency_(latency) {}

    /**
     * Utility function to parse a time specification.
     */
    bool parse_time(const char* time_str, double* time);

    /**
     * Utility function to fill in the values from a set of options
     * (e.g. bw=10kbps, latency=10ms).
     */
    bool parse_options(int argc, const char** argv, const char** invalidp);

    bool      open_;
    u_int64_t bw_;      // in bps
    double    latency_; // in seconds
};

/**
 * Base class for the underlying connectivity management between nodes
 * in the simulation.
 */
class Connectivity : public oasys::Logger {
public:
    /**
     * Singleton accessor.
     */
    static Connectivity* instance()
    {
        if (!instance_) {
            ASSERT(type_ != ""); // type must be set;
            instance_ = create_conn();
        }
        
        return instance_;
    }

    /**
     * Constructor.
     */
    Connectivity();

    /**
     * Destructor.
     */
    virtual ~Connectivity() {}
    
    /**
     * Get the current connectivity state between two nodes.
     */
    const ConnState* lookup(Node* n1, Node* n2);

protected:
    friend class ConnCommand;
    
    /**
     * The state structures are stored in a table indexed by strings
     * of the form NODE1,NODE2. Defaults can be set in the config with
     * a node name of '*' (and are stored in the table as such).
     */
    typedef oasys::StringHashMap<ConnState> StateTable;
    StateTable state_;

    /**
     * Static bootstrap initializer.
     */
    static Connectivity* create_conn();

    /**
     * Set the current connectivity state.
     */
    void set_state(const char* n1, const char* n2, const ConnState& s);
    
    static std::string type_;		///< The module type
    static Connectivity* instance_;	///< Singleton instance
};

} // namespace dtnsim

#endif /* _CONNECTIVITY_H_ */
