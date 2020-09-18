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

#ifndef _EHS_LINK_H_
#define _EHS_LINK_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <map>
#include <string.h>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include <oasys/debug/Logger.h>
#include <oasys/serialize/XercesXMLSerialize.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/TokenBucketLeaky.h>

#include "EhsEventHandler.h"
#include "EhsBundleTree.h"
#include "router-custom.h"

namespace dtn {


#define MAX_DESTINATIONS_PER_LINK   5
#define DEFAULT_LTP_PORT            1113

#define EHSLINK_BUNDLE_OVERHEAD     40


class EhsRouter;
class EhsLink;

// map of EHS Links
typedef std::map<std::string, EhsLink*> EhsLinkMap;
typedef std::pair<std::string, EhsLink*> EhsLinkPair;
typedef EhsLinkMap::iterator EhsLinkIterator;

// map of allowed incoming links by IP address and node ID
typedef std::map<std::string, uint64_t> EhsLinkAcceptMap;
typedef std::pair<std::string, uint64_t> EhsLinkAcceptPair;
typedef EhsLinkAcceptMap::iterator EhsLinkAcceptIterator;

// map of reachable nodes
typedef std::map<uint64_t, bool> EhsNodeBoolMap;
typedef EhsNodeBoolMap::iterator EhsNodeBoolIterator;

// map of next hop node IDs
typedef std::map<uint64_t, uint64_t> NextHopMap;
typedef NextHopMap::iterator NextHopIterator;


/// Structure to hold a Link's configuration
typedef struct EhsLinkCfg {
    std::string link_id_;
    std::string local_ip_addr_;
    uint16_t local_port_;
    bool is_fwdlnk_;
    bool establish_connection_;
    uint64_t throttle_bps_;
    EhsNodeBoolMap source_nodes_;
    EhsNodeBoolMap dest_nodes_;

    // NOTE: MAX_DESTINATIONS_PER_LINK is defined in EhsLink.h
    //XXX/dz this is to configure the LTPDUP CLA to service multiple
    //       destinations. transmit to the correct next hop based on 
    //       the destination of the bundle.
    //dz future for LTPUDP? int         num_destinations_;
    //dz future for LTPUDP? uint64_t    dest_node_id_[MAX_DESTINATIONS_PER_LINK];
    //dz future for LTPUDP? std::string dest_ip_addr_[MAX_DESTINATIONS_PER_LINK];
    //dz future for LTPUDP? uint16_t    dest_port_[MAX_DESTINATIONS_PER_LINK];

    EhsLinkCfg(std::string link_id)
    {
        link_id_ = link_id;
        local_ip_addr_ = "";
        local_port_ = 0;
        is_fwdlnk_ = false;
        establish_connection_ = false;;
        throttle_bps_ = 0;
        //dz future for LTPUDP? num_destinations_ = 0;
        //dz future for LTPUDP? for (int ix=0; ix<MAX_DESTINATIONS_PER_LINK; ix++) {
        //dz future for LTPUDP?     dest_node_id_[ix] = 0;
        //dz future for LTPUDP?     dest_ip_addr_[ix] = "";
        //dz future for LTPUDP?     dest_port_[ix] = 0;
        //dz future for LTPUDP? }
    }
} EhsLinkCfg;
typedef std::map<std::string, EhsLinkCfg*> EhsLinkCfgMap;
typedef EhsLinkCfgMap::iterator EhsLinkCfgIterator;


class EhsLink: public EhsEventHandler,
               public oasys::Thread
{
public:
    /**
     * Constructor for the different message types
     */
    EhsLink(rtrmessage::link_report::link::type& xlink, EhsRouter* parent, bool fwdlnk_aos);
    EhsLink(rtrmessage::link_opened_event& xlink, EhsRouter* parent, bool fwdlnk_aos);


    /**
     * Destructor.
     */
    virtual ~EhsLink();

    /**
     * Post events to be processed
     */
    virtual void post_event(EhsEvent* event, bool at_back=true);

    /**
     * Main event handling function.
     */
    virtual void handle_event(EhsEvent* event);
    virtual void event_handlers_completed(EhsEvent* event);

    /**
     * Report/Status methods
     */
    virtual void link_dump(oasys::StringBuffer* buf);

    /**
     * Set the Link state
     */
    virtual void set_link_state(const std::string state);

    /**
     * Parse info from the link opened event message
     */
    virtual void process_link_report(rtrmessage::link_report::link::type& xlink);
    virtual void process_link_available_event(rtrmessage::link_available_event& event);
    virtual void process_link_opened_event(rtrmessage::link_opened_event& event);
    virtual void process_link_unavailable_event(rtrmessage::link_unavailable_event& event);
    virtual void process_link_busy_event(rtrmessage::link_busy_event& event);
    virtual void process_link_closed_event(rtrmessage::link_closed_event& event);

    /**
     * Apply EHS Link configuration to the link
     */
    virtual void apply_ehs_cfg(EhsLinkCfg* linkcfg);

    virtual void apply_ehs_cfg(EhsLinkCfg& linkcfg) {
        apply_ehs_cfg(&linkcfg);
    }

    /**
     * Return newly disabled src-dst bundles to the Router's unrouted list
     */
    virtual uint64_t return_disabled_bundles(EhsBundleTree& unrouted_bundles, 
                                             EhsSrcDstWildBoolMap& fwdlink_xmt_enabled);

    /**
     * Return all bundles to the Router's unrouted list
     */
    virtual uint64_t return_all_bundles(EhsBundleTree& unrouted_bundles);
    virtual void return_all_bundles_to_router(EhsBundlePriorityTree& priority_tree);

    /**
     * Allow Sender to request a check for missed bundles
     */
    virtual void check_for_missed_bundles();

    /**
     * Inform Link & Sender that a bundle was transmitted
     */
    virtual void bundle_transmitted();

    /**
     * Check a node ID against acceptable Source and Dest IDs
     */
    virtual bool valid_source_node(uint64_t nodeid);
    virtual bool valid_dest_node(uint64_t nodeid);

    /**
     * Load buffer with details for display or logging
     */
    virtual void format_verbose(oasys::StringBuffer* buf);

    /**
     * Report/Status methods
     */
    virtual void set_link_statistics(bool enabled);

    /// @{ Accessors
    virtual std::string key()           { return link_id_; }
    virtual std::string link_id()       { return link_id_; }
    virtual std::string state()         { return state_; }
    virtual std::string remote_addr()   { return remote_addr_; }
    virtual bool is_open()              { return is_open_; }
    virtual bool is_rejected()          { return is_rejected_; }
    virtual bool is_fwdlnk()        { return is_fwdlnk_; }
    virtual uint64_t throttle_bps();
    virtual bool is_node_reachable(uint64_t node_id);
    virtual uint64_t ipn_node_id()      { return ipn_node_id_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_is_fwdlnk(bool t)      { is_fwdlnk_ = t; }
    virtual void set_fwdlnk_enabled_state(bool state);
    virtual void set_fwdlnk_aos_state(bool state);
    virtual void set_fwdlnk_force_LOS_while_disabled(bool force_los);
    virtual void set_throttle_bps(uint64_t rate);
    virtual void force_closed();
    virtual void set_is_rejected(bool t)        { is_rejected_ = t; }

    virtual void clear_sources();
    virtual void clear_destinations();

    virtual void add_source_node(uint64_t nodeid);
    virtual void add_destination_node(uint64_t node_id);
    //dz - future in LTPUDP? virtual bool add_destination(uint64_t node_id, std::string& ip_addr, uint16_t port);

    virtual void set_log_level(int t)         { log_level_ = t; }
    virtual void set_ipn_node_id(uint64_t nodeid);
    /// @}

protected:
    /**
     * Initialization
     */
    virtual void init();

    /**
     * Thread run method
     */
    virtual void run();

    /**
     * Check to see if the Link can currently send bundles
     */
    virtual bool okay_to_send();

    /**
     * Send the rate throttle via the External Router
     */
    virtual void apply_rate_throttle();

    /**
     * Send message to signal the Forward CLA of the AOS/LOS condition
     */
    virtual void send_reconfigure_link_comm_aos_msg();

    /**
     * Pass through to send a message to the DTN Server
     */
    virtual void send_msg(rtrmessage::bpa& msg);

    /**
     * Event handler methods
     */
    virtual void handle_route_bundle_req(EhsRouteBundleReq* event);
    virtual void handle_route_bundle_list_req(EhsRouteBundleListReq* event);
    virtual void handle_reconfigure_link_req(EhsReconfigureLinkReq* event);

    /**
     * Log message method for internal use - passes log info to parent
     */
    virtual void log_msg(oasys::log_level_t level, const char*format, ...);

    /**
     * Do Statistics reporting
     */
    virtual void do_stats();

protected:
    /**
     * Helper class (and thread) that sends the commands
     * to transmit a bundle when the stars are properly
     * aligned
     */
    class Sender : public oasys::Thread
    {
    public:
        /**
         * Constructor.
         */
        Sender(EhsLink* parent);

        /**
         * Destructor.
         */
        virtual ~Sender();
    
        /**
         * Queue a bundle to be transmitted
         */
        virtual void queue_bundle(EhsBundleRef& bref);
    
        /**
         * Queue a bundle list to be transmitted
         */
        virtual void queue_bundle_list(EhsBundlePriorityQueue* blist);

        /**
         * Return all bundles to the Router's unrouted list (because the link was disabled)
         */
        virtual uint64_t return_all_bundles(EhsBundleTree& unrouted_bundles);

        /**
         * Return newly disabled src-dst bundles to the Router's unrouted list
         */
        virtual uint64_t return_disabled_bundles(EhsBundleTree& unrouted_bundles, 
                                                 EhsSrcDstWildBoolMap& fwdlink_xmt_enabled);
        /**
         * Signals link was closed and bundles need to be returned to the Router
         */
        virtual void process_link_closed_event();

        /**
         * Reset check for missed bundles mechanism
         */
        virtual void reset_check_for_misseed_bundles();

        /**
         * Inform Sender that a bundle was transmitted
         */
        virtual void bundle_transmitted();

        /**
         * Set the throttle rate (0 = no throttle)
         */
        virtual void set_throttle_bps(uint64_t rate);

        /**
         * Get the current throttle rate
         */
        uint64_t throttle_bps() { return bucket_.rate(); }

        /**
         * Get the number of bundles to be sent
         */
        size_t priority_tree_size();

        /**
         * Get the current stats and clear them
         */
        void get_stats(uint64_t* bundles_sent, uint64_t* bytes_sent, uint64_t* send_waits);

    protected:
        /**
         * Main processing loop
         */
        virtual void run();

    protected:
        /// Lock for sequential access
        oasys::SpinLock lock_;

        /// Pointer back to the parent EhsLink
        EhsLink* parent_;

        /// Priority queue of bundles
        EhsBundlePriorityTree priority_tree_;

        /// Transmit parameters
        oasys::TokenBucketLeaky bucket_;

        /// Timer and flags to detect elapsed time to check for pending/missed bundles
        bool bundle_was_queued_;
        bool bundle_was_transmitted_;
        oasys::Time last_bundle_activity_;

        /// Statistics variables
        oasys::SpinLock stats_lock_;
        uint64_t stats_bundles_sent_;
        uint64_t stats_bytes_sent_;
        uint64_t send_waits_;
    };


protected:
    /// Lock for sequential access
    oasys::SpinLock lock_;

    /// Pointer to the parent EhsRouter
    EhsRouter* parent_;

    /// The event queue
    oasys::MsgQueue<EhsEvent*>* eventq_;

    /// Helper thread which manages the sending of bundles
    Sender* sender_;

    std::string link_id_;
    std::string remote_eid_;
    uint64_t ipn_node_id_;

    std::string state_;
    std::string reason_;

    std::string clayer_;
    std::string nexthop_;

    bool is_reachable_;
    bool is_usable_;
    uint32_t how_reliable_;
    uint32_t how_available_;
    uint32_t min_retry_interval_;
    uint32_t max_retry_interval_;
    uint32_t idle_close_time_;

    std::string local_addr_;
    uint16_t local_port_;
    std::string remote_addr_;
    uint16_t remote_port_;
    bool        segment_ack_enabled_;
    bool        negative_ack_enabled_;
    uint32_t    keepalive_interval_;
    uint32_t    segment_length_;
    uint32_t    busy_queue_depth_;
    bool        reactive_frag_enabled_;
    uint32_t    sendbuf_length_;
    uint32_t    recvbuf_length_;
    uint32_t    data_timeout_;
    uint32_t    rate_;
    uint32_t    bucket_depth_;
    uint32_t    channel_;

    /// Contact Attributes
    uint32_t start_time_sec_;
    uint32_t start_time_usec_;
    uint32_t duration_;
    uint32_t bps_;
    uint32_t latency_;
    uint32_t pkt_loss_prob_;

    /// calculated values
    bool is_open_;

    /// was the link rejected?
    bool is_rejected_;

    /// configured values
    bool is_fwdlnk_;

    /// Flag indicating Forward Link processing enabled
    bool fwdlnk_enabled_;

    /// Flag indicating AOS or LOS
    bool fwdlnk_aos_;

    /// Flag indicating AOS/LOS state needs to be sent to the DTNME daemon
    bool update_fwdlink_aos_;

    /// Flag indicating whether or not to force LOS condition on the 
    /// DTN Convergence Layer when disabled
    /// >> False allows sending LTP ACKS when AOS but no new bundles
    bool fwdlnk_force_LOS_while_disabled_;

    /// Flag to initiate a connection (TBD)
    bool establish_connection_;

    /// allowed source nodes from this link 
    /// (also identifies this link as the route for 
    ///  bundles destined to any of these nodes)
    EhsNodeBoolMap source_nodes_;


    /// allowed destination nodes from this link 
    EhsNodeBoolMap dest_nodes_;

    /// destination nodes and their respective address/port
    /// (for LTP with multiple connections)
    //dz - future in LTPUDP?  int         num_destinations_;
    //dz - future in LTPUDP? uint64_t    dest_node_id_[MAX_DESTINATIONS_PER_LINK];
    //dz - future in LTPUDP? std::string dest_ip_addr_[MAX_DESTINATIONS_PER_LINK];
    //dz - future in LTPUDP? uint16_t    dest_port_[MAX_DESTINATIONS_PER_LINK];

    /// Log level restriction low level oasys messages to stderr
    int log_level_;

    /// Statistics variables
    oasys::SpinLock stats_lock_;
    oasys::Time stats_timer_;
    bool stats_enabled_;
    uint64_t stats_bundles_received_;
    uint64_t stats_bytes_received_;
};

} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED

#endif /* _EHS_LINK_H_ */
