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

#ifndef _EHS_SRC_DST_WILD_BOOL_MAP_H_
#define _EHS_SRC_DST_WILD_BOOL_MAP_H_

#ifdef EHSROUTER_ENABLED

#include <map>

#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "EhsSrcDstKeys.h"

namespace dtn {


class EhsSrcDstWildBoolMap
{
protected:
    /**
     * Type for the map itself
     */
    typedef std::map<EhsSrcDstWildKey, bool, EhsSrcDstWildKey::mapcompare> List;
    typedef std::pair<EhsSrcDstWildKey, bool> Pair;
    typedef List::iterator Iterator;

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
    //typedef List::iterator iterator;

    /**
     * Constructor
     */
    EhsSrcDstWildBoolMap();

    /**
     * Destructor -- clears the list.
     */
    virtual ~EhsSrcDstWildBoolMap();

    /**
     * Clear out the list.
     */
    virtual void clear();

    /**
     * Return the size of the list.
     */
    virtual size_t size() const;

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const;

    /**
     * Add a source/dest pair to the list
     */
    virtual void put_pair(uint64_t source_node_id, uint64_t dest_node_id, bool accept=true);
    virtual void put_pair_wildcard_source(uint64_t dest_node_id, bool accept=true);
    virtual void put_pair_wildcard_dest(uint64_t source_node_id, bool accept=true);
    virtual void put_pair_double_wildcards(bool accept=true);

    /**
     * Clear a source/dest pair from the list
     */
    virtual void clear_pair(uint64_t source_node_id, uint64_t dest_node_id);
    virtual void clear_pair_wildcard_source(uint64_t dest_node_id);
    virtual void clear_pair_wildcard_dest(uint64_t source_node_id);
    virtual void clear_pair_double_wildcards();

    /**
     * Clear all references to a source node ID from the list
     */
    virtual void clear_source(uint64_t source_node_id);

    /**
     * Clear all references to a dest node ID from the list
     */
    virtual void clear_dest(uint64_t dest_node_id);

    /**
     * Check a source/dest pair to see if they are acceptable.
     * Looks for a specific match first and then tries wildcarding the
     * Source and then the Destination and finally both.
     */
    virtual bool check_pair(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Utility func to output the mapping
     */
    virtual void dump(oasys::StringBuffer* buf);

protected:
    List             list_;	  ///< underlying list data structure

    bool             default_accept_; ///< default acceptance if a match is not found (double wildcard)

    oasys::SpinLock  lock_;   ///< serialize access
};


} // namespace dtn


#endif // EHSROUTER_ENABLED

#endif /* _EHS_SRC_DST_WILD_BOOL_MAP_H_ */
