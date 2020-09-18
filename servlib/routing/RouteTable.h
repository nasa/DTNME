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

#ifndef _BUNDLE_ROUTETABLE_H_
#define _BUNDLE_ROUTETABLE_H_

#include <set>
#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/StringUtils.h>
#include <oasys/serialize/Serialize.h>

#include "RouteEntry.h"

namespace dtn {

/**
 * Class that implements the routing table, implemented
 * with an stl vector.
 */
class RouteTable : public oasys::Logger {
public:
    /**
     * Constructor
     */
    RouteTable(const std::string& router_name);

    /**
     * Destructor
     */
    virtual ~RouteTable();

    /**
     * Add a route entry.
     */
    bool add_entry(RouteEntry* entry);
    
    /**
     * Remove a route entry.
     */
    bool del_entry(const EndpointIDPattern& dest, const LinkRef& next_hop);

    /**
     * Remove entries that match the given predicate.
     */
    template<typename Predicate>
    size_t del_matching_entries(Predicate test);
    
    /**
     * Remove all entries to the given endpoint id pattern.
     *
     * @return the number of entries removed
     */
    size_t del_entries(const EndpointIDPattern& dest);

    /**
     * Remove all entries that rely on the given next_hop link
     *
     * @return the number of entries removed
     **/
    size_t del_entries_for_nexthop(const LinkRef& next_hop);

    /**
     * Clear the whole route table.
     */
    void clear();

    /**
     * Fill in the entry_vec with the list of all entries whose
     * patterns match the given eid and next hop. If the next hop is
     * NULL, it is ignored.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid, const LinkRef& next_hop,
                        RouteEntryVec* entry_vec) const;
    
    /**
     * Syntactic sugar to call get_matching for all links.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid,
                        RouteEntryVec* entry_vec) const
    {
        LinkRef link("RouteTable::get_matching: null");
        return get_matching(eid, link, entry_vec);
    }

    /**
     * Dump a string representation of the routing table.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Return the size of the table.
     */
    int size() { return route_table_.size(); }

    /**
     * Return the routing table.  Asserts that the RouteTable
     * spin lock is held by the caller.
     */
    const RouteEntryVec *route_table();

    /**
     * Accessor for the RouteTable internal lock.
     */
    oasys::Lock* lock() { return &lock_; }

protected:
    /// Helper function for get_matching
    size_t get_matching_helper(const EndpointID& eid,
                               const LinkRef&    next_hop,
                               RouteEntryVec*    entry_vec,
                               bool*             loop,
                               int               level) const;
    
    /// The routing table itself
    RouteEntryVec route_table_;

    /**
     * Lock to protect internal data structures.
     */
    mutable oasys::SpinLock lock_;
};

//----------------------------------------------------------------------
template <typename Predicate>
inline size_t
RouteTable::del_matching_entries(Predicate pred)
{
    oasys::ScopeLock l(&lock_, "RouteTable::del_matching_entries");

    // XXX/demmer there should be a way to avoid having to go through
    // twice but i can't figure it out...
    bool found = false;
    for (RouteEntryVec::iterator iter = route_table_.begin();
         iter != route_table_.end(); ++iter)
    {
        if (pred(*iter)) {
            found = true;
            delete *iter;
            *iter = NULL;
        }
    }

    // short circuit if nothing was deleted
    if (!found) 
        return 0;

    size_t old_size = route_table_.size();

    // this stl ugliness first sorts the vector so all the null
    // entries are at the end, then cleans them out with erase()
    RouteEntryVec::iterator new_end =
        std::remove_if(route_table_.begin(), route_table_.end(),
                       std::bind2nd(std::equal_to<RouteEntry*>(), 0));
    route_table_.erase(new_end, route_table_.end());
    
    return old_size - route_table_.size();
}

} // namespace dtn

#endif /* _BUNDLE_ROUTETABLE_H_ */
