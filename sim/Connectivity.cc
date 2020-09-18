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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "Connectivity.h"
#include "Node.h"
#include "SimEvent.h"
#include "SimConvergenceLayer.h"
#include "Topology.h"

#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/EndpointID.h"

namespace dtnsim {

//----------------------------------------------------------------------
Connectivity* Connectivity::instance_(NULL);
std::string   Connectivity::type_("");

//----------------------------------------------------------------------
Connectivity::Connectivity()
    : Logger("Connectivity", "/sim/conn")
{
}

//----------------------------------------------------------------------
Connectivity*
Connectivity::create_conn()
{
    ASSERT(type_ != "");

    if (type_ == "static") {
        // just the base class
        return new Connectivity(); 
    } else {
        log_crit_p("/connectivity", "invalid connectivity module type %s",
                   type_.c_str());
        return NULL;
    }
}

//----------------------------------------------------------------------
bool
ConnState::parse_time(const char* time_str, double* time)
{
    char* end;
    *time = strtod(time_str, &end);

    if (end == time_str)
        return false;

    if (!strcmp(end, "us")) {
        *time = *time / 1000000.0;
        return true;
        
    } else if (!strcmp(end, "ms")) {
        *time = *time / 1000.0;
        return true;

    } else if (!strcmp(end, "s")) {
        return true;

    } else if (!strcmp(end, "min")) {
        *time = *time * 60;
        return true;

    } else if (!strcmp(end, "hr")) {
        *time = *time * 3600;
        return true;

    } else {
        return false;
    }
}

//----------------------------------------------------------------------
bool
ConnState::parse_options(int argc, const char** argv, const char** invalidp)
{
    oasys::OptParser p;
    std::string latency_str;

    p.addopt(new oasys::RateOpt("bw", &bw_));
    p.addopt(new oasys::StringOpt("latency", &latency_str));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    if (latency_str != "" && !parse_time(latency_str.c_str(), &latency_)) {
        *invalidp = strdup(latency_str.c_str()); // leak!
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
void
Connectivity::set_state(const char* n1, const char* n2, const ConnState& s)
{
    log_debug("set state %s,%s: %s bw=%llu latency=%f",
              n1, n2, s.open_ ? "up" : "down", U64FMT(s.bw_), s.latency_);
    
    // handle wildcards
    if (!strcmp(n1, "*")) {
        Topology::NodeTable* nodes = Topology::node_table();
        
        for (Topology::NodeTable::iterator iter = nodes->begin();
             iter != nodes->end(); ++iter)
        {
            if (strcmp(iter->second->name(), n2) != 0) {
                set_state(iter->second->name(), n2, s);
            }
        }
        return;
    }

    if (!strcmp(n2, "*")) {
        Topology::NodeTable* nodes = Topology::node_table();
        
        for (Topology::NodeTable::iterator iter = nodes->begin();
             iter != nodes->end(); ++iter)
        {
            if (strcmp(n1, iter->second->name()) != 0) {
                set_state(n1, iter->second->name(), s);
            }
        }
        return;
    }
    
    oasys::StringBuffer key("%s,%s", n1, n2);
    StateTable::iterator iter = state_.find(key.c_str());
    if (iter != state_.end()) {
        iter->second = s;
    } else {
        state_[key.c_str()] = s;
    }
    
    SimConvergenceLayer::instance()->
        update_connectivity(Topology::find_node(n1),
                            Topology::find_node(n2), s);
}

//----------------------------------------------------------------------
const ConnState*
Connectivity::lookup(Node* n1, Node* n2)
{
    oasys::StringBuffer key("%s,%s", n1->name(), n2->name());
    StateTable::iterator iter = state_.find(key.c_str());
    if (iter == state_.end()) {
        return NULL;
    }
    return &iter->second;
}

} // namespace dtnsim
