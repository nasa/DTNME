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
#  include <dtn-config.h>
#endif

#ifdef EHSROUTER_ENABLED

#include <inttypes.h>

#include "EhsSrcDstWildBoolMap.h"

namespace dtn {


//----------------------------------------------------------------------
EhsSrcDstWildBoolMap::EhsSrcDstWildBoolMap()
{
    // set the default acceptance to false
    put_pair_double_wildcards(false);
}

//----------------------------------------------------------------------
EhsSrcDstWildBoolMap::~EhsSrcDstWildBoolMap()
{
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildBoolMap::check_pair(uint64_t source_node_id, uint64_t dest_node_id)
{
    bool result = default_accept_;
    EhsSrcDstWildKey key(source_node_id, dest_node_id);

    // look for an exact match first
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::put_pair");

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        result = iter->second;
    } else {
        // look for a wildcard destination match
        key.wildcard_dest_ = true;
        key.dest_node_id_ = 0;
        iter = list_.find(key);
        if (iter != list_.end()) {
            result = iter->second;
        } else {
            // look for a wildcard source match
            key.wildcard_dest_ = false;
            key.dest_node_id_ = dest_node_id;
            key.wildcard_source_ = true;
            key.source_node_id_ = 0;
            iter = list_.find(key);
            if (iter != list_.end()) {
                result = iter->second;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::put_pair(uint64_t source_node_id, uint64_t dest_node_id, bool accept)
{
    EhsSrcDstWildKey key(source_node_id, dest_node_id);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::put_pair");
    list_[key] = accept;
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::put_pair_wildcard_source(uint64_t dest_node_id, bool accept)
{
    EhsSrcDstWildKey key(true, dest_node_id);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::put_pair_wildcard_source");
    list_[key] = accept;
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::put_pair_wildcard_dest(uint64_t source_node_id, bool accept)
{
    EhsSrcDstWildKey key(source_node_id, true);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::put_pair_wildcard_dest");
    list_[key] = accept;
}

//----------------------------------------------------------------------
void 
EhsSrcDstWildBoolMap::put_pair_double_wildcards(bool accept)
{
    // avoid the lookup on the double wildcards and just maintain a default value
    default_accept_ = accept;
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::clear_pair(uint64_t source_node_id, uint64_t dest_node_id)
{
    EhsSrcDstWildKey key(source_node_id, dest_node_id);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_pair");
    list_.erase(key);
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::clear_pair_wildcard_source(uint64_t dest_node_id)
{
    EhsSrcDstWildKey key(true, dest_node_id);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_pair_wildcard_source");
    list_.erase(key);
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::clear_pair_wildcard_dest(uint64_t source_node_id)
{
    EhsSrcDstWildKey key(source_node_id, true);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_pair_wildcard_dest");
    list_.erase(key);
}

//----------------------------------------------------------------------
void 
EhsSrcDstWildBoolMap::clear_pair_double_wildcards()
{
    EhsSrcDstWildKey key(true, true);
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_pair_double_wildcards");
    list_.erase(key);
}

//----------------------------------------------------------------------
void 
EhsSrcDstWildBoolMap::clear_source(uint64_t source_node_id)
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_source");
    Iterator del_iter;
    Iterator iter = list_.begin();
    while (iter != list_.end())
    {
        if (!iter->first.wildcard_source_ && 
            iter->first.source_node_id_ == source_node_id) {
            // delete this entry
            del_iter = iter;
            ++iter;
            list_.erase(del_iter);
        } else {
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void 
EhsSrcDstWildBoolMap::clear_dest(uint64_t dest_node_id)
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear_dest");
    Iterator del_iter;
    Iterator iter = list_.begin();
    while (iter != list_.end())
    {
        if (!iter->first.wildcard_dest_ && 
            iter->first.dest_node_id_ == dest_node_id) {
            // delete this entry
            del_iter = iter;
            ++iter;
            list_.erase(del_iter);
        } else {
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::clear()
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::clear");
    list_.clear();
}


//----------------------------------------------------------------------
size_t
EhsSrcDstWildBoolMap::size() const
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::size");
    return list_.size();
}

//----------------------------------------------------------------------
bool
EhsSrcDstWildBoolMap::empty() const
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::empty");
    return (0 == list_.size());
}

//----------------------------------------------------------------------
void
EhsSrcDstWildBoolMap::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, "EhsSrcDstWildBoolMap::dump");

#define ACCEPTSTR accept?"true":"false"

    bool accept = false;
    bool src_changed = false;
    bool have_range = false;
    bool output_range = false;

    uint64_t src_node = 0;
    uint64_t dst_begin = 0;
    uint64_t dst_end = 0;

    bool wild_src = false;
    bool wild_dst = false;

    Iterator iter = list_.begin();
    while (iter != list_.end())
    {
        src_changed = ((iter->first.wildcard_source_ != wild_src) ||
                       (iter->first.source_node_id_ != src_node) ||
                       (accept != iter->second));

        output_range = src_changed && have_range;
        if (!output_range && have_range) {
            if (iter->first.wildcard_dest_ != wild_dst) 
                output_range = true;
            else if (iter->first.dest_node_id_ == dst_end + 1)
                ++dst_end;
            else 
                output_range = true;
        }

        
        if (output_range) {
            // output the current range
            if (wild_src) {
                if (dst_end > dst_begin)
                    buf->appendf("    * -> %" PRIu64 " - %" PRIu64 "   %s\n", dst_begin, dst_end, ACCEPTSTR);
                else
                    buf->appendf("    * -> %" PRIu64 "  %s\n", dst_begin, ACCEPTSTR);
            } else {
                if (wild_dst)
                    buf->appendf("    %" PRIu64 " -> *  %s\n", src_node, ACCEPTSTR);
                else if (dst_end > dst_begin)
                    buf->appendf("    %" PRIu64 " -> %" PRIu64 " - %" PRIu64 "  %s\n", src_node, dst_begin, dst_end, ACCEPTSTR);
                else
                    buf->appendf("    %" PRIu64 " -> %" PRIu64 "  %s\n", src_node, dst_begin, ACCEPTSTR);
            }
            src_changed = false;
            have_range = false;
            output_range = false;
        } 

        if (!have_range) {
            wild_src = iter->first.wildcard_source_;
            src_node = iter->first.source_node_id_;

            wild_dst = iter->first.wildcard_dest_;
            dst_begin = iter->first.dest_node_id_;
            dst_end = dst_begin;
            
            have_range = true;
        }

        accept = iter->second;
            
        ++iter;
    }

    if (have_range) {
        // output the last range
        if (wild_src) {
            if (dst_end > dst_begin)
                buf->appendf("    * -> %" PRIu64 " - %" PRIu64 "  %s\n", dst_begin, dst_end, ACCEPTSTR);
            else
                buf->appendf("    * -> %" PRIu64 "  %s\n", dst_begin, ACCEPTSTR);
        } else {
            if (wild_dst)
                buf->appendf("    %" PRIu64 " -> *  %s\n", src_node, ACCEPTSTR);
            else if (dst_end > dst_begin)
                buf->appendf("    %" PRIu64 " -> %" PRIu64 " - %" PRIu64 "  %s\n", src_node, dst_begin, dst_end, ACCEPTSTR);
            else
                buf->appendf("    %" PRIu64 " -> %" PRIu64 "  %s\n", src_node, dst_begin, ACCEPTSTR);
        }
    } 

    buf->appendf("    Default result is %s\n", default_accept_?"TRUE":"FALSE");
}

} // namespace dtn


#endif // EHSROUTER_ENABLED
