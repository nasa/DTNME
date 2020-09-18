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

#ifndef _LINK_H_
#define _LINK_H_

#include <set>
#include <oasys/debug/Formatter.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/Ref.h>
#include <oasys/util/RefCountedObject.h>

#include "bundling/BundleList.h"
#include "naming/EndpointID.h"

#include "Contact.h"
#include "NamedAttribute.h"

namespace dtn {

class ConvergenceLayer;
class CLInfo;
class Contact;
class Link;
class RouterInfo;

/**
 * Typedef for a reference on a link.
 */
typedef oasys::Ref<Link> LinkRef;

/**
 * Set of links
 */
class LinkSet : public std::set<LinkRef> {};

/**
 * Abstraction for a DTN link, i.e. a one way communication channel to
 * a next hop node in the DTN overlay.
 *
 * The state of a link (regarding its availability) is described by
 * the Link::state_t enumerated type.
 *
 * All links in the OPEN state have an associated contact that
 * represents an actual connection.
 *
 * Every link has a unique name associated with it which is used to
 * identify it. The name is configured explicitly when the link is
 * created.
 *
 * Creating new links:
 * Links are created explicitly in the configuration file. Syntax is:
 * @code
 * link add <name> <nexthop> <type> <conv_layer> <args>
 * @endcode
 * See servlib/cmd/LinkCommand.cc for implementation of this TCL cmd.
 *
 * ----------------------------------------------------------
 *
 * Links are of three types as discussed in the DTN architecture
 * ONDEMAND, SCHEDULED, OPPORTUNISTIC.
 *
 * The key differences from an implementation perspective are "who"
 * and "when" manipulates the link state regarding availability.
 *
 * ONDEMAND links are initializd in the AVAILABLE state, as one would
 * expect. It remains in this state until a router explicitly opens
 * it.
 *
 * An ONDEMAND link can then be closed either due to connection
 * failure or because the link has been idle for too long, both
 * triggered by the convergence layer. If an ONDEMAND link is closed
 * due to connection failure, then the contact manager is notified of
 * this event and periodically tries to re-establish the link.
 *
 * For OPPORTUNISTIC links the availability state is set by the code
 * which detects that there is a new link available to be used. 
 *
 * SCHEDULED links have their availability dictated by the schedule
 * implementation.
 *
 * ----------------------------------------------------------
 *
 * Links are used for input and/or output. In other words, for
 * connection-oriented convergence layers like TCP, a link object is
 * created whenever a new connection is made to a peer or when a
 * connection arrives from a peer. 
 */
class Link : public oasys::RefCountedObject,
             public oasys::Logger,
             public oasys::SerializableObject {
public:
    /**
     * Valid types for a link.
     */
    typedef enum
    {
        LINK_INVALID = -1,
        
        /**
         * The link is expected to be ALWAYS available, and any
         * convergence layer connection state is always maintained for
         * it.
         */
        ALWAYSON = 1,
        
        /**
         * The link is expected to be either always available, or can
         * be made available easily. Examples include DSL (always),
         * and dialup (easily available). Convergence layers are free
         * to tear down idle connection state, but are expected to be
         * able to easily re-establish it.
         */
        ONDEMAND = 2,
        
        /**
         * The link is only available at pre-determined times.
         */
        SCHEDULED = 3,
        
        /**
         * The link may or may not be available, based on
         * uncontrollable factors. Examples include a wireless link
         * whose connectivity depends on the relative locations of the
         * two nodes.
         */
        OPPORTUNISTIC = 4
    }
    link_type_t;

    /**
     * Link type string conversion.
     */
    static inline const char*
    link_type_to_str(link_type_t type)
    {
        switch(type) {
        case ALWAYSON:		return "ALWAYSON";
        case ONDEMAND:		return "ONDEMAND";
        case SCHEDULED:		return "SCHEDULED";
        case OPPORTUNISTIC: 	return "OPPORTUNISTIC";
        default: 		PANIC("bogus link_type_t");
        }
    }

    static inline link_type_t
    str_to_link_type(const char* str)
    {
        if (strcasecmp(str, "ALWAYSON") == 0)
            return ALWAYSON;
        
        if (strcasecmp(str, "ONDEMAND") == 0)
            return ONDEMAND;
        
        if (strcasecmp(str, "SCHEDULED") == 0)
            return SCHEDULED;
        
        if (strcasecmp(str, "OPPORTUNISTIC") == 0)
            return OPPORTUNISTIC;
        
        return LINK_INVALID;
    }

    /**
     * The possible states for a link. These are defined as distinct
     * bitfield values so that various functions can match on a set of
     * states (e.g. see ContactManager::find_link_to).
     */
    typedef enum {
        UNAVAILABLE = 1,///< The link is closed and not able to be
                        ///  opened currently.

        AVAILABLE = 2,	///< The link is closed but is able to be
                        ///  opened, either because it is an on demand
                        ///  link, or because an opportunistic peer
                        ///  node is in close proximity but no
                        ///  convergence layer session has yet been
                        ///  opened.
        
        OPENING = 4,	///< A convergence layer session is in the
                        ///  process of being established.
        
        OPEN = 8,	///< A convergence layer session has been
                        ///  established, and the link has capacity
                        ///  for a bundle to be sent on it. This may
                        ///  be because no bundle is currently being
                        ///  sent, or because the convergence layer
                        ///  can handle multiple simultaneous bundle
                        ///  transmissions.
        
        CLOSED = 16	///< Bogus state that's never actually used in
                        ///  the Link state_ variable, but is used for
                        ///  signalling the daemon thread with a
                        ///  LinkStateChangeRequest
        
    } state_t;

    /**
     * Convert a link state into a string.
     */
    static inline const char*
    state_to_str(state_t state)
    {
        switch(state) {
        case UNAVAILABLE: 	return "UNAVAILABLE";
        case AVAILABLE: 	return "AVAILABLE";
        case OPENING: 		return "OPENING";
        case OPEN: 		return "OPEN";
        case CLOSED: 		return "CLOSED";
        }

        NOTREACHED;
    }
    
    /**
     * Static function to create appropriate link object from link type.
     */
    static LinkRef create_link(const std::string& name, link_type_t type,
                               ConvergenceLayer* cl, const char* nexthop,
                               int argc, const char* argv[],
                               const char** invalid_argp = NULL);

    /**
     * Constructor / Destructor
     */
    Link(const std::string& name, link_type_t type,
         ConvergenceLayer* cl, const char* nexthop);

    /**
     * Constructor for unserialization.
     */
    Link(const oasys::Builder& b);

    /**
     * Handle and mark deleted link.
     */
    virtual void delete_link();

    /**
     *
     */
    virtual void cancel_all_bundles();

    /**
     * Reconfigure the link parameters.
     */
    virtual bool reconfigure_link(int argc, const char* argv[]);

    virtual void reconfigure_link(AttributeVector& params);

    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* action);

    /**
     * Hook for subclass to parse arguments.
     */
    virtual int parse_args(int argc, const char* argv[],
                           const char** invalidp = NULL);

    /**
     * Hook for subclass to post events to control the initial link
     * state, after all initialization is complete.
     */
    virtual void set_initial_state();

    /**
     * Return the type of the link.
     */
    link_type_t type() const { return static_cast<link_type_t>(type_); }
    bool is_opportunistic() { return type_ == OPPORTUNISTIC; }

    /**
     * Return the string for of the link.
     */
    const char* type_str() const { return link_type_to_str(type()); }

    /**
     * Return whether or not the link is open.
     */
    bool isopen() const { return ( (state_ == OPEN) ); }

    /**
     * Return the availability state of the link.
     */
    bool isavailable() const { return (state_ != UNAVAILABLE); }

    /**
     * Return whether the link is in the process of opening.
     */
    bool isopening() const { return (state_ == OPENING); }

    /**
     * Returns true if the link has been deleted; otherwise returns false.
     */
    bool isdeleted() const;

    /**
     * Return the actual state.
     */
    state_t state() const { return static_cast<state_t>(state_); }

    /**
     * Sets the state of the link. Performs various assertions to
     * ensure the state transitions are legal.
     *
     * This function should only ever be called by the main
     * BundleDaemon thread and helper classes. All other threads must
     * use a LinkStateChangeRequest event to cause changes in the link
     * state.
     *
     * The function isn't protected since some callers (i.e.
     * convergence layers) are not friend classes but some functions
     * are run by the BundleDaemon thread.
     */
    void set_state(state_t state);

    /**
     * Set/get the create_pending_ flag on the link.
     */
    void set_create_pending(bool create_pending = true)
             { create_pending_ = create_pending; }
    bool is_create_pending() const { return create_pending_; }

    /**
     * Set/get the usable_ flag on the link.
     */
    void set_usable(bool usable = true) { usable_ = usable; }
    bool is_usable() const { return usable_; }

    /**
     * Return the current contact information (if any).
     */
    const ContactRef& contact() const { return contact_; }

    /**
     * Set the contact information.
     */
    void set_contact(Contact* contact)
    {
        // XXX/demmer check this invariant
        ASSERT(contact_ == NULL);
        contact_ = contact;
    }

    /**
     * Store convergence layer state associated with the link.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }

    /**
     * Accessor to the convergence layer state.
     */
    CLInfo* cl_info() const { return cl_info_; }
    
    /**
     * Store router state associated with the link.
     */
    void set_router_info(RouterInfo* router_info)
    {
        ASSERT((router_info_ == NULL && router_info != NULL) ||
               (router_info_ != NULL && router_info == NULL));
        
        router_info_ = router_info;
    }

    /**
     * Accessor to the convergence layer state.
     */
    RouterInfo* router_info() const { return router_info_; }
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer() const { return clayer_; }

    /**
     * Accessor to this links name - its unique identifier
     */
    const char* name() const { return name_.c_str(); }

    /**
     * Accessors for unique link name when used as datastore table key
     */
    std::string durable_key() const { return name_; }

    /**
     * Accessor to this links name as a c++ string
     */
    const std::string& name_str() const { return name_; }

    /**
     * Accessor to convergence layer name string
     */
    const char* cl_name() const { return cl_name_.c_str(); }

    /**
     * Accessor to convergence layer name as a C++ string
     */
    const std::string& cl_name_str() const { return cl_name_; }

    /**
     * Accessor to next hop string
     */
    const char* nexthop() const { return nexthop_.c_str(); }

    /**
     * Accessor to next hop string
     */
    const std::string& nexthop_str() const { return nexthop_; }

    /**
     * Override for the next hop string.
     */
    void set_nexthop(const std::string& nexthop) { nexthop_.assign(nexthop); }

    /**
     * Accessor to the reliability bit.
     */
    bool is_reliable() const { return reliable_; }

    /**
     * Accessor to set the reliability bit when the link is created.
     */
    void set_reliable(bool r) { reliable_ = r; }

    /**
     * Accessor to set the remote endpoint id.
     */
    void set_remote_eid(const EndpointID& remote) {
        remote_eid_.assign(remote);
    }

    /**
     * Accessor to check if this link was reincarnated.
     * Used to choose addition or updating of persistent store.
     */
    bool reincarnated() { return reincarnated_; }

    /**
     * Accessor to set reincarnated_, recoding that this link specification
     * had a previous incarnation, so that its persistent store just needs
     * updating rather than creating..
     */
    void set_reincarnated() { reincarnated_ = true; }

    /**
     * Accessor to check if this link has been referenced in a bundle
     * forwarding log.
     */
    bool used_in_fwdlog() { return used_in_fwdlog_; }

    /**
     * Accessor to set used_in_fwdlog recording that the link has been referenced
     * in one or more bundle forwarding logs. Forces update of persistent store
     * when used_in_fwdlog_ transitions from false to true.
     */
    void set_used_in_fwdlog(void);

    /**
     * Accessor to the remote endpoint id.
     */
    const EndpointID& remote_eid() { return remote_eid_; }

    /**
     * Accessor for the link's queue of bundles that are awaiting
     * transmission.
     */
    const BundleList* queue() { return &queue_; }

    /**
     * Return whether or not the queue is full, based on the
     * configured queue limits.
     */
    bool queue_is_full() const;
    
    /**
     * Return whether or not the queue has space, based on the
     * configured queue limits.
     */
    bool queue_has_space() const;

    /**
     * Return whether or not the queue limits are active. 
     */
    bool queue_limits_active () const;

    /**
     * In datastore flag getter and setter
     */
    bool in_datastore() const          { return in_datastore_; }
    void set_in_datastore(bool t)      { in_datastore_ = t; }

    /**
     * Track number of bundles on the deferred list and
     * if positive periodically kick off an event to see if
     * they can be queued for transmission
     */
    void increment_deferred_count();
    void decrement_deferred_count();

    /**
     * Accessor for the link's list of bundles that have been
     * transmitted but for which the convergence layer is awaiting
     * acknowledgement.
     */
    const BundleList* inflight() { return &inflight_; }

    /// @{
    /**
     * Accessor functions to add/remove bundles from the link queue
     * and inflight list, keeping the statistics in-sync with the
     * state of the lists.
     */
    bool add_to_queue(const BundleRef& bundle, size_t total_len);
    bool del_from_queue(const BundleRef& bundle, size_t total_len);
    bool add_to_inflight(const BundleRef& bundle, size_t total_len);
    bool del_from_inflight(const BundleRef& bundle, size_t total_len);
    /// @}
    
    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz) const;

    /**
     * Debugging printout.
     */
    void dump(oasys::StringBuffer* buf);

    /**************************************************************
     * Link Parameters
     */
    struct Params {
        /**
         * Default constructor.
         */
        Params();
        
        /**
         * MTU of the link, used to control proactive fragmentation.
         */
        u_int mtu_;
        /**
         * Minimum amount to wait between attempts to re-open the link
         * (in seconds).
         *
         * Default is set by the various Link types but can be overridden
         * by configuration parameters.
         */
        u_int min_retry_interval_;
    
        /**
         * Maximum amount to wait between attempts to re-open the link
         * (in seconds).
         *
         * Default is set by the various Link types but can be overridden
         * by configuration parameters.
         */
        u_int max_retry_interval_;

        /**
         * Seconds of idle time before the link is closed. Must be
         * zero for always on links (i.e. they are never closed).
         *
         * Default is 30 seconds for on demand links, zero for
         * opportunistic links.
         */
        u_int idle_close_time_;

        /**
         * Conservative estimate of the maximum amount of time that
         * the link may be down during "normal" operation. Used by
         * routing algorithms to determine how long to leave bundles
         * queued on the down link before rerouting them. Fefault is
         * 30 seconds.
         */
        u_int potential_downtime_;

        /**
         * Whether or not to send the previous hop header on this
         * link. Default is false.
         */
        bool prevhop_hdr_;

        /**
         * Abstract cost of the link, used by routing algorithms.
         * Default is 100.
         */
        u_int cost_;

        /** @{
         *
         * Configurable high / low limits on the number of
         * bundles/bytes that should be queued on the link.
         *
         * The high limits are used by Link::is_queue_full() to
         * indicate whether or not more bundles can be queued onto the
         * link to effect backpressure from the convergence layers.
         *
         * The low limits can be used by the router to determine when
         * to re-scan the pending bundle lists 
         */
        bool qlimit_enabled_;
        size_t qlimit_bundles_high_;
        size_t qlimit_bytes_high_;
        size_t qlimit_bundles_low_;
        size_t qlimit_bytes_low_;
        /// @}
    };
    
    /**
     * Seconds to wait between attempts to re-open an unavailable
     * link. Initially set to min_retry_interval_, then doubles up to
     * max_retry_interval_.
     */
    u_int retry_interval_;

    /**
     * Accessor for the parameter structure.
     */
    const Params& params() const { return params_; }
    Params& params() { return params_; }

    /*************************************************************
     * Link Statistics
     */
    struct Stats {
        /**
         * Number of times the link attempted to be opened.
         */
        size_t contact_attempts_;
        
        /**
         * Number of contacts ever successfully opened on the link
         * (equivalent to the number of times the link was open)
         */
        size_t contacts_;

        /**
         * Number of bundles transmitted over the link.
         */
        size_t bundles_transmitted_;

        /**
         * Total byte count transmitted over the link.
         */
        size_t bytes_transmitted_;

        /**
         * Number of bundles with cancelled transmission.
         */
        size_t bundles_cancelled_;

        /**
         * The total uptime of the link, not counting the current
         * contact.
         */
        size_t uptime_;

        /**
         * The availablity of the link, as measured over time by the
         * convergence layer.
         */
        size_t availability_;
        
        /**
         * The reliability of the link, as measured over time by the
         * convergence layer.  This is different from the is_reliable
         * setting, which indicates whether the convergence layer should
         * expect acks from the peer.
         */
        size_t reliability_;
    };

    /**
     * Accessor for the stats structure.
     */
    Stats* stats() { return &stats_; }

    /**
     * Reset the stats.
     */
    void reset_stats() const
    {
        memset(&stats_, 0, sizeof(stats_));
    }

    /**
     * Dump a printable version of the stats.
     */
    void dump_stats(oasys::StringBuffer* buf);

    /// @{ Accessors for the link queue stats
    size_t bundles_queued()   { return bundles_queued_; }
    size_t bytes_queued()     { return bytes_queued_; }
    size_t bundles_inflight() { return bundles_inflight_; }
    size_t bytes_inflight()   { return bytes_inflight_; }
    /// @}

    /**
     * Accessor for the Link state lock.
     */
    oasys::Lock* lock() { return &lock_; }
    
    virtual ~Link();
protected:
    friend class BundleActions;
    friend class BundleDaemon;
    friend class ContactManager;
    friend class ParamCommand;

    /**
     * Open the link. Protected to make sure only the friend
     * components can call it and virtual to allow subclasses to
     * override it.
     */
    virtual void open();

    /**
     * Close the link. Protected to make sure only the friend
     * components can call it and virtual to allow subclasses to
     * override it.
     */
    virtual void close();

    /**
     * Trigger checking to see if deferred bundles can be sent
     */
    virtual void check_deferred_bundles();


    /// Type of the link
    int type_;

    /// State of the link
    u_int32_t state_;

    /// Flag, that when set to true, indicates that the link has been deleted.
    bool deleted_;

    /// Flag, that when set to true, indicates that the creation of the
    /// link is pending; the convergence layer will post a creation event
    /// when the creation is complete. While creation is pending, the
    /// link cannot be opened nor can bundles be queued for it.
    bool create_pending_;

    /// Flag, that when set to true, indicates that the link is allowed
    /// to be used to transmit bundles.
    bool usable_;

    /// Next hop address
    std::string nexthop_;
    
    /// Internal name of the link 
    std::string name_;

    /// Whether or not this link is reliable
    bool reliable_;

    /// Parameters of the link
    Params params_;

    /// Default parameters of the link
    static Params default_params_;

    /// Lock to protect internal data structures and state.
    oasys::SpinLock lock_;
    
    /// Queue of bundles currently active or pending transmission on the Link
    BundleList queue_;

    /// Queue of bundles that have been sent but not yet acknowledged
    BundleList inflight_;
    
    /** @{
         *
     * Data counters about the link queues, both in terms of bundles
     * and bytes.
     *
     *    *_queued:       the link queue size
     *    *_inflight:     transmitted but not yet acknowledged
     */
    size_t bundles_queued_;
    size_t bytes_queued_; 
    size_t bundles_inflight_;
    size_t bytes_inflight_;
    /** @} */

    /// Stats for the link
    mutable Stats stats_;

    /// Current contact. contact_ != null iff link is open
    ContactRef contact_;

    /// Pointer to convergence layer
    ConvergenceLayer* clayer_;

    /// Name of convergence layer
    std::string cl_name_;

    /// Convergence layer specific info, if needed
    CLInfo* cl_info_;

    /// Router specific info, if needed
    RouterInfo* router_info_;

    /// Remote's endpoint ID (eg, dtn://hostname.dtn)
    EndpointID remote_eid_;

    /// Flag indicating that this link is a reincarnation of a closely similar
    /// link that existed during a previous run.  The name, nexthop, type and
    /// convergence layer are the same but some other parameters may have been
    /// varied depending on whether it was automatically or manually recreated
    /// and whether or not the defaults have changed.  Used to determine whether
    /// persistent storage should be added or updated.
    bool reincarnated_;

    /// Flag indicating if this link (name) has been referenced in a bundle
    /// forwarding log.  If so, then the link will remain in persistent
    /// storage even if it is deleted and the name cannot be reused unless
    /// the same nexthop, type and convergence layer are specified.  If not,
    /// the link can be deleted completely and the name reused (to allow
    /// command input finger trouble to be corrected).
    bool used_in_fwdlog_;

    /// Flag indicating if the Link is in the persistent store
    bool in_datastore_;

    class LinkDeferredTimer : public oasys::Timer,
                              public oasys::Logger 
    {
    public:
        LinkDeferredTimer( Link* link )
                : Logger("Link::LinkDeferredTimer",
                         "/dtn/link/deferredtimer")
        {
            linkref_ = link;
        }
  
        /**
         * Timer callback function
         */
        void timeout(const struct timeval &now);

    public:
        LinkRef  linkref_;
    };


    /// Number of bundles deferred 
    size_t deferred_bundle_count_;

    /// Timer to generate a BundleCheckDeferredEvent
    LinkDeferredTimer* deferred_timer_;

    /// Destructor -- protected since links shouldn't be deleted
    //virtual ~Link();
};

} // namespace dtn

#endif /* _LINK_H_ */
