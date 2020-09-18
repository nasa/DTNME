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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleRouter.h"
#include "RouteTable.h"

namespace dtn {

//----------------------------------------------------------------------
RouteTable::RouteTable(const std::string& router_name)
    : Logger("RouteTable", "/dtn/routing/%s/table", router_name.c_str())
{
}

//----------------------------------------------------------------------
RouteTable::~RouteTable()
{
    RouteEntryVec::iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        delete *iter;
    }
}

//----------------------------------------------------------------------
bool
RouteTable::add_entry(RouteEntry* entry)
{
    oasys::ScopeLock l(&lock_, "RouteTable::add_entry");
    
    route_table_.push_back(entry);
    
    return true;
}

//----------------------------------------------------------------------
bool
RouteTable::del_entry(const EndpointIDPattern& dest, const LinkRef& next_hop)
{
    oasys::ScopeLock l(&lock_, "RouteTable::del_entry");

    RouteEntryVec::iterator iter;
    RouteEntry* entry;

    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        if (entry->dest_pattern().equals(dest) && entry->link() == next_hop) {
            log_debug("del_entry *%p", entry);
            
            route_table_.erase(iter);
            delete entry;
            return true;
        }
    }    

    log_debug("del_entry %s -> %s: no match!",
              dest.c_str(), next_hop->nexthop());
    return false;
}

//----------------------------------------------------------------------
size_t
RouteTable::del_entries(const EndpointIDPattern& dest)
{
    oasys::ScopeLock l(&lock_, "RouteTable::del_entries");
    return del_matching_entries(RouteEntry::DestMatches(dest));
}

//----------------------------------------------------------------------
size_t
RouteTable::del_entries_for_nexthop(const LinkRef& next_hop)
{
    oasys::ScopeLock l(&lock_, "RouteTable::del_entries_for_nexthop");
    return del_matching_entries(RouteEntry::NextHopMatches(next_hop));
}

//----------------------------------------------------------------------
void
RouteTable::clear()
{
    oasys::ScopeLock l(&lock_, "RouteTable::clear");

    RouteEntryVec::iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        delete *iter;
    }
    route_table_.clear();
}

//----------------------------------------------------------------------
size_t
RouteTable::get_matching(const EndpointID& eid,
                         const LinkRef& next_hop,
                         RouteEntryVec* entry_vec) const
{
    oasys::ScopeLock l(&lock_, "RouteTable::get_matching");

    bool loop = false;
    log_debug("get_matching %s (link %s)...", eid.c_str(),
              next_hop != NULL ? next_hop->name() : "NULL");
    size_t ret = get_matching_helper(eid, next_hop, entry_vec, &loop, 0);
    if (loop) {
        log_warn("route destination %s caused route table lookup loop",
                 eid.c_str());
    }
    return ret;
}

//----------------------------------------------------------------------
size_t
RouteTable::get_matching_helper(const EndpointID& eid,
                                const LinkRef&    next_hop,
                                RouteEntryVec*    entry_vec,
                                bool*             loop,
                                int               level) const
{
    RouteEntryVec::const_iterator iter;
    RouteEntry* entry;
    size_t count = 0;

    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter)
    {
        entry = *iter;

        log_debug("RouteTable::get_matching_helper: check entry *%p", entry);
        
        if ((strstr(eid.uri().c_str(),"*") != NULL) && (strcmp(entry->dest_pattern().uri().c_str(),eid.uri().c_str()) == 0)) {
          //XXX/dz drop through on special case to match wildcards in both uri(s)
 
        } else if (! entry->dest_pattern().match(eid)) {
            continue;
        }
        
        if (entry->link() == NULL)
        {
            ASSERT(entry->route_to().length() != 0);

            if (level >= BundleRouter::config_.max_route_to_chain_) {
                *loop = true;
                continue;
            }
            
            count += get_matching_helper(entry->route_to(), next_hop,
                                         entry_vec, loop, level + 1);
        }
        else if (next_hop == NULL || entry->link() == next_hop)
        {
            if (std::find(entry_vec->begin(), entry_vec->end(), entry) == entry_vec->end()) {
                log_debug("match entry *%p", entry);
                entry_vec->push_back(entry);
                ++count;
            } else {
                log_debug("entry *%p already in matches... ignoring", entry);
            }
        }
    }

    log_debug("get_matching %s done (level %d), %zu match(es)", eid.c_str(), level, count);
    return count;
}

//----------------------------------------------------------------------
void
RouteTable::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(&lock_, "RouteTable::dump");
    
    oasys::StringVector long_strings;

    // calculate appropriate lengths for the long strings
    size_t dest_eid_width   = 10;
    size_t source_eid_width = 6;
    size_t next_hop_width   = 10;

    RouteEntryVec::const_iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        RouteEntry* e = *iter;
        dest_eid_width   = std::max(dest_eid_width,
                                    e->dest_pattern().length());
        source_eid_width = std::max(source_eid_width,
                                    e->source_pattern().length());
        next_hop_width   = std::max(next_hop_width,
                                    (e->link() != NULL) ?
                                    e->link()->name_str().length() :
                                    e->route_to().length());
    }

    dest_eid_width   = std::min(dest_eid_width, (size_t)25);
    source_eid_width = std::min(source_eid_width, (size_t)15);
    next_hop_width   = std::min(next_hop_width, (size_t)15);
    
    RouteEntry::dump_header(buf, dest_eid_width, source_eid_width, next_hop_width);
    
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        (*iter)->dump(buf, &long_strings,
                      dest_eid_width, source_eid_width, next_hop_width);
    }
    
    if (long_strings.size() > 0) {
        buf->appendf("\nLong EIDs/Links referenced above:\n");
        for (u_int i = 0; i < long_strings.size(); ++i) {
            buf->appendf("\t[%d]: %s\n", i, long_strings[i].c_str());
        }
        buf->appendf("\n");
    }
    
    buf->append("\nClass of Service (COS) bits:\n"
                "\tB: Bulk  N: Normal  E: Expedited\n\n");
}

//----------------------------------------------------------------------
const RouteEntryVec *
RouteTable::route_table()
{
    ASSERTF(lock_.is_locked_by_me(),
            "RouteTable::route_table must be called while holding lock");
    return &route_table_;
}

} // namespace dtn
