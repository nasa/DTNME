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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#ifndef _BUNDLE_ROUTEENTRY_H_
#define _BUNDLE_ROUTEENTRY_H_

#include <oasys/debug/Formatter.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/util/StringUtils.h>

#include "bundling/CustodyTimer.h"
#include "bundling/ForwardingInfo.h"
#include "naming/EndpointID.h"
#include "contacts/Link.h"

namespace dtn {

class RouteEntryInfo;

/**
 * Class to represent route table entry.
 *
 * Each entry contains an endpoint id pattern that is matched against
 * the destination address in the various bundles to determine if the
 * route entry should be used for the bundle.
 *
 * An entry also has a forwarding action type code which indicates
 * whether the bundle should be forwarded to this next hop and others
 * (ForwardingInfo::COPY_ACTION) or sent only to the given next hop
 * (ForwardingInfo::FORWARD_ACTION). The entry also stores the custody
 * transfer timeout parameters, unique for a given route.
 *
 * There is also a pointer to either an interface or a link for each
 * entry. In case the entry contains a link, then that link will be
 * used to send the bundle. If there is no link, there must be an
 * interface. In that case, bundles which match the entry will cause
 * the router to create a new link to the given endpoint whenever a
 * bundle arrives that matches the route entry. This new link is then
 * typically added to the route table.
 */
class RouteEntry : public oasys::Formatter,
                   public oasys::SerializableObject {
public:
    /// Share the ForwardingInfo::action_t type
    typedef ForwardingInfo::action_t action_t;

    /// Functor classes used to match route entries (defined below)
    class DestMatches;
    class NextHopMatches;

    /**
     * First constructor requires a destination pattern and a next hop link.
     */
    RouteEntry(const EndpointIDPattern& dest_pattern, const LinkRef& link);

    /**
     * Alternate constructor requires a destination pattern and a
     * route destination endpoint id.
     */
    RouteEntry(const EndpointIDPattern& dest_pattern,
               const EndpointIDPattern& route_to);

    /**
     * Destructor.
     */
    ~RouteEntry();

    /**
     * Hook to parse route configuration options.
     */
    int parse_options(int argc, const char** argv,
                      const char** invalidp = NULL);

    /**
     * Virtual from formatter.
     */
    int format(char* buf, size_t sz) const;
     
    /**
     * Dump a header string in preparation for subsequent calls to dump();
     */
    static void dump_header(oasys::StringBuffer* buf,
                            int dest_eid_width,
                            int source_eid_width,
                            int next_hop_width);
    
    /**
     * Dump a string representation of the route entry. Any endpoint
     * ids or link names that don't fit into the column width get put
     * into the long_strings vector.
     */
    void dump(oasys::StringBuffer* buf,
              oasys::StringVector* long_strings,
              int dest_eid_width,
              int source_eid_width,
              int next_hop_width) const;

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize( oasys::SerializeAction *a );

    /// @{ Accessors
    const EndpointIDPattern& dest_pattern()   const { return dest_pattern_; }
    const EndpointIDPattern& source_pattern() const { return source_pattern_; }
    const LinkRef&           link()           const { return link_; }
    const EndpointIDPattern& route_to()       const { return route_to_; }
    u_int                    priority()       const { return priority_; }
    RouteEntryInfo*          info()           const { return info_; }
    const CustodyTimerSpec&  custody_spec()   const { return custody_spec_; }

    action_t action() const { return static_cast<action_t>(action_); }

    const std::string& next_hop_str() const {
        return (link() != NULL) ? link()->name_str() : route_to().str();
    }
    /// @}

    /// @{ Setters
    void set_action(action_t action)    { action_ = action; }
    void set_info(RouteEntryInfo* info) { info_   = info; }
    /// @}
    
private:
    /// Helper for dump()
    static void append_long_string(oasys::StringBuffer* buf,
                                   oasys::StringVector* long_strings,
                                   int width, const std::string& str);

    
    /// The pattern that matches bundles' destination eid
    EndpointIDPattern dest_pattern_;

    /// The pattern that matches bundles' source eid
    EndpointIDPattern source_pattern_;
    
    /// Bit vector of the bundle priority classes that should match this route
    u_int bundle_cos_;
    
    /// Route priority
    u_int priority_;

    /// Next hop link if known
    LinkRef link_;
        
    /// Route destination for recursive lookups
    EndpointIDPattern route_to_;
        
    /// Forwarding action code 
    u_int32_t action_;

    /// Custody timer specification
    CustodyTimerSpec custody_spec_;

    /// Abstract pointer to any algorithm-specific state that needs to
    /// be stored in the route entry
    RouteEntryInfo* info_;        
    
    // XXX/demmer confidence? latency? capacity?
    // XXX/demmer bit to distinguish
    // XXX/demmer make this serializable?
};

/**
 * Predicate to match the destination pattern for a route.
 */
class RouteEntry::DestMatches {
public:
    DestMatches(const EndpointIDPattern& dest) : dest_(dest) {}
    bool operator()(RouteEntry* entry) {
        return dest_.equals(entry->dest_pattern());
    }
    EndpointIDPattern dest_;
};

/**
 * Predicate to match the destination pattern for a route.
 */
class RouteEntry::NextHopMatches {
public:
    NextHopMatches(const LinkRef& link) : link_(link) {}
    bool operator()(RouteEntry* entry) {
        return link_ == entry->link();
    }
    LinkRef link_;
};

/**
 * Interface for any per-entry routing algorithm state.
 */
class RouteEntryInfo {
public:
    virtual ~RouteEntryInfo() {}
};

/**
 * Class for a vector of route entries. Used for the route table
 * itself and for what is returned in get_matching().
 */
class RouteEntryVec : public std::vector<RouteEntry*> {};

/**
 * Functor class to sort a vector of routes based on forwarding
 * priority, using the bytes queued on the link to break ties.
 */
struct RoutePrioritySort {
    bool operator() (RouteEntry* a, RouteEntry* b) {
        // XXX/dz the bytes_queued() is now a moving target which
        // causes a segfault in the std::sort call if the values change 
        // between comparisons
        //if (a->priority() < b->priority()) return false;
        //if (a->priority() > b->priority()) return true;
        //return (a->link()->bytes_queued() <
        //        b->link()->bytes_queued());

        return (a->priority() > b->priority());
    }
};

} // namespace dtn

#endif /* _BUNDLE_ROUTEENTRY_H_ */
