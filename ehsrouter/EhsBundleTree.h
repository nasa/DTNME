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

#ifndef _EHS_BUNDLE_TREE_H_
#define _EHS_BUNDLE_TREE_H_


#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <map>

#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>

#include "EhsBundleRef.h"
#include "EhsExternalRouter.h"
#include "EhsSrcDstKeys.h"
#include "EhsSrcDstWildBoolMap.h"

namespace dtn {

class EhsLink;


// Object to hold a list of bundles and track statistics by source-dest pairing
class EhsBundleMapWithStats
{
public:

    /**
     * Constructor
     */
    EhsBundleMapWithStats();

    /**
     * Destructor -- clears the list.
     */
    virtual ~EhsBundleMapWithStats();

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
     * Get/Set the max expiration time (TTL) for tracking abusers
     */
    virtual uint64_t max_expiration_fwd()               { return max_expiration_fwd_; }
    virtual void set_max_expiration_fwd(uint64_t secs)  { max_expiration_fwd_ = secs; }
    virtual uint64_t max_expiration_rtn()               { return max_expiration_rtn_; }
    virtual void set_max_expiration_rtn(uint64_t secs)  { max_expiration_rtn_ = secs; }
    
    /**
     * Bundle maintenance methods
     */
    virtual EhsBundleRef find(uint64_t bundleid);
    virtual bool bundle_received(EhsBundleRef& bref);
    virtual void erase_bundle(EhsBundleRef& bref);
    virtual EhsBundleRef bundle_expired(uint64_t bundleid);
    virtual void bundle_rejected(EhsBundleRef& bref);
    virtual EhsBundleRef bundle_custody_accepted(uint64_t bundleid);
    virtual EhsBundleRef bundle_custody_released(uint64_t bundleid);
    virtual EhsBundleRef bundle_transmitted(uint64_t bundleid, bool success);
    virtual EhsBundleRef bundle_delivered(uint64_t bundleid);

    /**
     * Utility reporting functions
     */
    virtual void bundle_list(oasys::StringBuffer* buf);
    virtual void bundle_stats(oasys::StringBuffer* buf, 
                              uint64_t undelivered, uint64_t unrouted);
    virtual void dump(oasys::StringBuffer* buf);
    virtual void bundle_stats_by_src_dst(int* count, EhsBundleStats** stats);
    virtual void fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats);
    virtual void get_bundle_stats(uint64_t* received, uint64_t* transmitted, uint64_t* transmit_failed,
                                  uint64_t* delivered, uint64_t* rejected,
                                  uint64_t* pending, uint64_t* custody);
    virtual void bundle_id_list(EhsBundleMap& bidlist, uint64_t source_node_id, uint64_t dest_node_id);
    virtual void bundle_id_list(EhsBundleMap& bidlist);


    
protected:
    /**
     * Utility methods to update statistics
     */
    virtual void inc_stats(EhsBundleRef& bref);
    virtual void dec_stats(EhsBundleRef& bref);

    virtual void inc_transmitted(EhsBundleRef& bref);
    virtual void inc_transmit_failed(EhsBundleRef& bref);
    virtual void inc_expired(EhsBundleRef& bref);
    virtual void inc_delivered(EhsBundleRef& bref);
    virtual void inc_rejected(EhsBundleRef& bref);
    virtual void inc_custody(EhsBundleRef& bref);
    virtual void dec_custody(EhsBundleRef& bref);

protected:
    /**
     * Types for the list of bundles and stats
     */
    typedef EhsBundleMap List;
    typedef EhsBundlePair Pair;
    typedef EhsBundleIterator Iterator;

    typedef std::map<EhsSrcDstKey, EhsBundleStats*, EhsSrcDstKey::mapcompare> StatsMap;
    typedef std::pair<EhsSrcDstKey, EhsBundleStats*> StatsPair;
    typedef StatsMap::iterator StatsIterator;


    oasys::SpinLock lock_;   ///< serialize access

    List list_;    ///< underlying list data structure
    StatsMap stats_map_;   ///< Maintain statistics by src-dst

    uint64_t max_expiration_fwd_; ///< Max allowed expiration time (Time To Live or TTL)
    uint64_t max_expiration_rtn_; ///< Max allowed expiration time (Time To Live or TTL)

    uint64_t total_pending_;
    uint64_t total_custody_;
    uint64_t total_bytes_;

    uint64_t total_received_;
    uint64_t total_transmitted_;
    uint64_t total_transmit_failed_;
    uint64_t total_rejected_;
    uint64_t total_delivered_;
    uint64_t total_expired_;

    uint64_t total_expedited_rcv_;
    uint64_t total_expedited_xmt_;

    uint64_t total_pending_bulk_;
    uint64_t total_pending_normal_;
    uint64_t total_pending_expedited_;
    uint64_t total_pending_reserved_;
};



// Object to hold the bundles for a given Source-Dest combo in
// priority order and track statistics
class EhsBundlePriorityQueue
{
public:
    /**
     * Constructors
     */
    EhsBundlePriorityQueue(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Destructor.
     */
    virtual ~EhsBundlePriorityQueue();

    /**
     * Insert a bundle returns true if inserted else false
     */
    virtual bool insert_bundle(EhsBundleRef& bref);

    /**
     * Pop off the top bundle
     */
    virtual EhsBundleRef pop_bundle();

    /**
     * Peek at the first bundle in the list without popping it
     */
    virtual EhsBundleRef peek_bundle();

    /**
     * Get the priority key of the first bundle
     */
    virtual std::string first_bundle_priority();

    /**
     * Clear out the list.
     */
    virtual void clear();

    /**
     * Return the size of the list.
     */
    virtual size_t size() const;

    /**
     * Return the total bundle bytes in the list
     */
    virtual uint64_t total_bytes() const  { return total_bytes_; }

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const;

    /**
     * Utility func to output the tree
     */
    virtual void dump(oasys::StringBuffer* buf);

    /**
     * Utility methods to update statistics
     */
    virtual void inc_stats(EhsBundleRef& bref);
    virtual void dec_stats(EhsBundleRef& bref);

    /**
     * Update peripheral stats
     */
    virtual void inc_received();
    virtual void inc_transmitted();
    virtual void inc_expired();
    virtual void inc_delivered();
    virtual void inc_rejected();
    virtual void inc_custody();
    virtual void dec_custody();

public:
    oasys::SpinLock  lock_;   ///< serialize access

    uint64_t source_node_id_;
    uint64_t dest_node_id_;

    uint64_t total_received_;
    uint64_t total_transmitted_;
    uint64_t total_rejected_;
    uint64_t total_delivered_;
    uint64_t total_expired_;

    uint64_t total_pending_;
    uint64_t total_custody_;
    uint64_t total_bytes_;

    uint64_t total_expedited_rcv_;
    uint64_t total_expedited_xmt_;

    uint64_t total_pending_bulk_;
    uint64_t total_pending_normal_;
    uint64_t total_pending_expedited_;
    uint64_t total_pending_reserved_;

    EhsBundleStrMap list_;
};


// Object to hold the a list of Src-Dest Priority Queues in Src-Dest order
class EhsBundleTree
{
public:

    /**
     * Constructor
     */
    EhsBundleTree();

    /**
     * Destructor -- clears the list.
     */
    virtual ~EhsBundleTree();

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
     * Insert a bundle
     */
    virtual void insert_bundle(EhsBundleRef& bref);

    /**
     * Insert a bundle
     */
    virtual void insert_queue(EhsBundlePriorityQueue* queue);

    /**
     * Queue bundles to the link
     */
    virtual uint64_t route_to_link(EhsLink* el, EhsSrcDstWildBoolMap& fwdlink_xmt_enabled);

    /**
     * Retrieves the EhsBundlePriorityQueue for a given Source-Dest combo
     * NOTE: You need to delete it after finished with it
     */
    virtual EhsBundlePriorityQueue* extract_bundle_list(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Update peripheral stats
     */
    virtual void inc_received(EhsBundleRef& bref);
    virtual void inc_transmitted(EhsBundleRef& bref);
    virtual void inc_expired(EhsBundleRef& bref);
    virtual void inc_delivered(EhsBundleRef& bref);
    virtual void inc_rejected(EhsBundleRef& bref);
    virtual void inc_custody(EhsBundle* eb);
    virtual void dec_custody(EhsBundle* eb);


    /**
     * Utility func to output the tree
     */
    virtual void dump(oasys::StringBuffer* buf);

    
protected:
    /**
     * Utility methods to update statistics
     */
    virtual void inc_stats(EhsBundleRef& bref);
    virtual void inc_stats(EhsBundlePriorityQueue* blist);
    virtual void dec_stats(EhsBundlePriorityQueue* blist);

protected:
    /**
     * Type for the map of bundle lists
     */
    typedef std::map<EhsSrcDstKey, EhsBundlePriorityQueue*, EhsSrcDstKey::mapcompare> List;
    typedef std::pair<EhsSrcDstKey, EhsBundlePriorityQueue*> Pair;
    typedef List::iterator Iterator;



    oasys::SpinLock  lock_;   ///< serialize access

    List             list_;	  ///< underlying list data structure

    uint64_t total_pending_;
    uint64_t total_custody_;
    uint64_t total_bytes_;

    uint64_t total_received_;
    uint64_t total_transmitted_;
    uint64_t total_rejected_;
    uint64_t total_delivered_;
    uint64_t total_expired_;

    uint64_t total_expedited_rcv_;
    uint64_t total_expedited_xmt_;

    uint64_t total_pending_bulk_;
    uint64_t total_pending_normal_;
    uint64_t total_pending_expedited_;
    uint64_t total_pending_reserved_;
};



class NodePriorityMap
{
public:

    /**
     * Constructor
     */
    NodePriorityMap();

    /**
     * Destructor -- clears the list.
     */
    virtual ~NodePriorityMap();

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
     * Set the default priority
     */
    virtual void set_default_priority(int priority);

    /**
     * Set a Node ID's priority
     */
    virtual void set_node_priority(uint64_t node_id, int priority);

    /**
     * Retrieve the priority for a Node ID
     */
    virtual int get_node_priority(uint64_t node_id);

    /**
     * Utility func to output the tree
     */
    virtual void dump(oasys::StringBuffer* buf);

protected:
    /**
     * Type for the map of bundle lists by Src-Dest
     */
    typedef std::map<uint64_t, int> Map;
    typedef std::pair<uint64_t, int> Pair;
    typedef Map::iterator Iterator;

    oasys::SpinLock  lock_;   ///< serialize access

    Map priority_map_;       ///< underlying list data structure

    /// default priority value - higher value is higher priority
    int default_priority_;
};


// Object to hold the a list of Src-Dest Priority Queues in priority order
class EhsBundlePriorityTree
{
public:

    /**
     * Constructor
     */
    EhsBundlePriorityTree();

    /**
     * Destructor -- clears the list.
     */
    virtual ~EhsBundlePriorityTree();

    /**
     * Clear out the list.
     */
    virtual void clear();

    /**
     * Return the size of the list.
     */
    virtual size_t size() const;

    /**
     * Return the total bundle bytes in the list
     */
    virtual uint64_t total_bytes() const  { return total_bytes_; }

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const;

    /**
     * Set a Source Node ID's priority
     */
    virtual void set_src_node_priority(uint64_t node_id, int priority);

    /**
     * Set a Destination Node ID's priority
     */
    virtual void set_dst_node_priority(uint64_t node_id, int priority);

    /**
     * Insert a bundle
     */
    virtual void insert_bundle(EhsBundleRef& bref);

    /**
     * Insert a bundle
     */
    virtual void insert_queue(EhsBundlePriorityQueue* queue);

    /**
     * Retrieve the highest priority bundle
     */
    virtual EhsBundleRef pop_bundle();

    /**
     * Return newly disabled src-dst bundles to the Router's unrouted list
     */
    virtual uint64_t return_disabled_bundles(EhsBundleTree& tree, 
                                             EhsSrcDstWildBoolMap& fwdlink_xmt_enabled);

    /**
     * Return all bundles to the Router's unrouted list
     */
    virtual uint64_t return_all_bundles(EhsBundleTree& tree);

    /**
     * Utility func to output the tree
     */
    virtual void dump(oasys::StringBuffer* buf);

protected:
    /**
     * Utility function to generate the priority key of list
     */
    virtual char* build_key(EhsBundlePriorityQueue* blist);

    /**
     * Utility methods to update statistics
     */
    virtual void inc_stats(EhsBundleRef& bref);
    virtual void dec_stats(EhsBundleRef& bref);
    virtual void inc_stats(EhsBundlePriorityQueue* blist);
    virtual void dec_stats(EhsBundlePriorityQueue* blist);

protected:
    /**
     * Type for the map of bundle lists by Src-Dest
     */
    typedef std::map<EhsSrcDstKey, EhsBundlePriorityQueue*, EhsSrcDstKey::mapcompare> SrcDstMap;
    typedef std::pair<EhsSrcDstKey, EhsBundlePriorityQueue*> SrCDestPair;
    typedef SrcDstMap::iterator SrcDstIterator;

    /**
     * Type for the map of bundle lists by priority
     */
    typedef std::map<std::string, EhsBundlePriorityQueue*> PriorityMap;
    typedef std::pair<std::string, EhsBundlePriorityQueue*> PriorityPair;
    typedef PriorityMap::iterator PriorityIterator;


    oasys::SpinLock  lock_;   ///< serialize access

    SrcDstMap src_dst_map_;        ///< underlying list data structure
    PriorityMap priority_map_;      ///< underlying list data structure

    NodePriorityMap src_priority_;  ///< list of priority for source node IDs
    NodePriorityMap dst_priority_; ///< list of priority for destination node IDs;


    uint64_t total_pending_;
    uint64_t total_bytes_;

    uint64_t total_pending_bulk_;
    uint64_t total_pending_normal_;
    uint64_t total_pending_expedited_;
    uint64_t total_pending_reserved_;

    char key_buf_[48];
};


// Object to hold bundles by Global Bundle or Fragment ID (gbofid_str)
class EhsBundleList
{
public:
    /**
     * Constructors
     */
    EhsBundleList();

    /**
     * Destructor.
     */
    virtual ~EhsBundleList();

    /**
     * Insert a bundle returns true if inserted else false
     */
    virtual bool insert_bundle(EhsBundleRef& bref);

    /**
     * Delete a bundle by its key (GbofID)
     */
    virtual void delete_bundle(std::string& key);

    /**
     * Clear out the list.
     */
    virtual void clear();

    /**
     * Housekeeping to remove expired bundles
     */
    virtual void clear_expired_bundles();

    /**
     * Return the size of the list.
     */
    virtual size_t size() const;

    /**
     * Return whether or not the list is empty.
     */
    virtual bool empty() const;

    /**
     * Utility func to output the tree
     */
    virtual void dump(oasys::StringBuffer* buf);

public:
    oasys::SpinLock  lock_;   ///< serialize access

    EhsBundleStrMap list_;
};

} // namespace dtn


#endif // defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#endif // EHSROUTER_ENABLED

#endif /* _EHS_BUNDLE_TREE_H_ */
