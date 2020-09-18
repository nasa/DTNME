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

#ifndef _EHS_DTN_NODE_H_
#define _EHS_DTN_NODE_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <map>
#include <string.h>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/serialize/XercesXMLSerialize.h>

#include "EhsBundleRef.h"
#include "EhsBundleTree.h"
#include "EhsEventHandler.h"
#include "EhsLink.h"
#include "EhsSrcDstWildBoolMap.h"
#include "router-custom.h"

namespace dtn {

class EhsDtnNode;
class EhsExternalRouterImpl;
class EhsLink;
class EhsRouter;


// map of DTN Nodes
typedef std::map<std::string, EhsDtnNode*> EhsDtnNodeMap;
typedef std::pair<std::string, EhsDtnNode*> EhsDtnNodePair;
typedef EhsDtnNodeMap::iterator EhsDtnNodeIterator;

// map of source-dest node IDs for accepting bundles
//typedef std::map<std::string, bool> AcceptBundlesMap;
//typedef std::pair<std::string, bool> AcceptBundlesPair;
//typedef AcceptBundlesMap::iterator AcceptBundlesIterator;

// map of source node IDs for accepting custody of bundles
//typedef std::map<int64_t, bool> AcceptCustodyMap;
//typedef std::pair<int64_t, bool> AcceptCustodyPair;
//typedef AcceptCustodyMap::iterator AcceptCustodyIterator;

typedef std::map<uint64_t, uint64_t> DeliveredBundleIDList;
typedef std::pair<uint64_t, uint64_t> DeliveredBundleIDPair;
typedef DeliveredBundleIDList::iterator DeliveredBundleIDIterator;


class EhsDtnNode: public EhsEventHandler,
                  public oasys::Thread
{
public:
    /**
     * Constructor
     */
    EhsDtnNode(EhsExternalRouterImpl* parent, std::string eid, std::string ipn_eid);

    /**
     * Destructor.
     */
    virtual ~EhsDtnNode();

    /**
     * Post events to be processed
     */
    virtual void post_event(EhsEvent* event, bool at_back=true);

    /**
     * Main event handling functions
     */
    virtual void handle_event(EhsEvent* event);
    virtual void event_handlers_completed(EhsEvent* event);

    /**
     * Pass through a message to the EhsExernalRouter to be sent to the DTNME node
     */
    virtual void send_msg(rtrmessage::bpa& msg);

    /**
     * Issue shutdown request to the DTNME server
     */
    virtual bool shutdown_server();

    //
    // Bundle management functions
    //
    /**
     * Delete bundles that match the specified source and destination node ID
     */
    virtual int bundle_delete(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Delete all bundles in the DTN database
     */
    virtual int bundle_delete_all();

    /**
     * EhsLink initiatedcheck for missed bundles
     */
    virtual uint64_t check_for_missed_bundles(uint64_t ipn_node_id);

    /**
     * Report/Status methods
     */
    virtual void update_statistics(uint64_t* received, uint64_t* transmitted, uint64_t* transmit_failed,
                                   uint64_t* delivered, uint64_t* rejected,
                                   uint64_t* pending, uint64_t* custody, 
                                   bool* fwdlnk_aos, bool* fwdlnk_enabled);
    virtual void bundle_stats(oasys::StringBuffer* buf);
    virtual void bundle_list(oasys::StringBuffer* buf);
    virtual void link_dump(oasys::StringBuffer* buf);
    virtual void fwdlink_transmit_dump(oasys::StringBuffer* buf);
    virtual void bundle_stats_by_src_dst(int* count, EhsBundleStats** stats);
    virtual void fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats);
    virtual void unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf);
    virtual void set_link_statistics(bool enabled);

    /**
     * Accessors
     */
    virtual const std::string* eid()          { return &eid_; }
    virtual const std::string* eid_ping()     { return &eid_ping_; }
    virtual const std::string* eid_ipn()      { return &eid_ipn_; }
    virtual const std::string* eid_ipn_ping() { return &eid_ipn_ping_; }
    virtual bool prime_mode()                 { return prime_mode_; }

    virtual bool is_local_node(uint64_t check_node);
    virtual bool is_local_node(std::string& check_eid);
    virtual bool is_local_admin_eid(std::string check_eid);

    /**
     * Setters
     */
    virtual void set_prime_mode(bool prime_mode);
    virtual void reconfigure_link(std::string link_id);
    virtual void reconfigure_accept_custody();
    virtual void reconfigure_max_expiration_fwd(uint64_t exp);
    virtual void reconfigure_max_expiration_rtn(uint64_t exp);
    virtual void reconfigure_source_priority();
    virtual void reconfigure_dest_priority();
    virtual void set_fwdlnk_enabled_state(bool state);
    virtual void set_fwdlnk_aos_state(bool state);
    virtual void set_fwdlnk_throttle(uint32_t bps);

    virtual void fwdlink_transmit_enable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list);
    virtual void fwdlink_transmit_disable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list);
    virtual void set_fwdlnk_force_LOS_while_disabled(bool force_los);
    virtual void link_enable(EhsLinkCfg* cl);
    virtual void link_disable(std::string& linkid);

    /**
     * Log message methods which format and pass messages back to the application
     */
    virtual void set_log_level(int t);
    virtual void log_msg(const std::string& path, oasys::log_level_t level, const char* format, ...);
    virtual void log_msg(const std::string& path, oasys::log_level_t level, std::string& msg);
    virtual void log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args);

    /**
     * Pass through the EhsLink to get its configuration info
     */
    virtual bool get_link_configuration(EhsLink* el);
    virtual void get_fwdlink_transmit_enabled_list(EhsSrcDstWildBoolMap& enabled_list);

    virtual void get_source_node_priority(NodePriorityMap& pmap);
    virtual void get_dest_node_priority(NodePriorityMap& pmap);

protected:
    /**
     * Main thread function that dispatches events.
     */
    void run();

    /**
     * Updates to the BUNDLES_BY_DEST_MAP
     */
    void add_bundle_by_dest(EhsBundleRef& bref);
    void del_bundle_by_dest(EhsBundleRef& bref);

    /**
     * Log message method for internal use
     */
    virtual void log_msg(oasys::log_level_t level, const char* format, ...);

    /**
     * Event handler methods
     */
    virtual void handle_bpa_received(EhsBpaReceivedEvent* event);
    virtual void handle_free_bundle_req(EhsFreeBundleReq* event);
//    virtual void handle_delete_bundle_req(EhsDeleteBundleReq* event);

    /**
     * Message processing methods
     */
    virtual void process_bundle_report(rtrmessage::bpa* msg);
    virtual void process_bundle_received_event(rtrmessage::bpa* msg);
    virtual void process_bundle_custody_accepted_event(rtrmessage::bpa* msg);
    virtual void process_bundle_expired_event(rtrmessage::bpa* msg);
    virtual void process_data_transmitted_event(rtrmessage::bpa* msg);
    virtual void process_bundle_send_cancelled_event(rtrmessage::bpa* msg);
    virtual void process_bundle_delivered_event(rtrmessage::bpa* msg);
    virtual void process_custody_signal_event(rtrmessage::bpa* msg);
    virtual void process_aggregate_custody_signal_event(rtrmessage::bpa* msg);
    virtual void process_custody_timeout_event(rtrmessage::bpa* msg);

    virtual void finalize_bundle_delivered_event(uint64_t id);

    /**
     * Utility methods
     */
    virtual void route_bundle(EhsBundleRef& bref);
    virtual bool is_local_destination(EhsBundleRef& bref);

    virtual bool accept_custody_before_routing(EhsBundleRef& bref);

    virtual void inc_bundle_stats_by_src_dst(EhsBundle* eb);
    virtual void dec_bundle_stats_by_src_dst(EhsBundle* eb);

protected:
    /// Lock for sequential access
    oasys::SpinLock lock_;

    /// Lock for sequential access
    oasys::SpinLock seqctr_lock_;

    /// EID of the DTN Node
    std::string eid_;

    /// ping EID of the DTN Node
    std::string eid_ping_;

    /// EID (IPN scheme) of the DTN Node
    std::string eid_ipn_;

    /// ping (service number 2047 for ISS) EID (IPN scheme) of the DTN Node
    std::string eid_ipn_ping_;

    /// IPN scheme node ID of the DTN Node
    uint64_t node_id_;

    /// Parent EhsExternalRouterImpl
    EhsExternalRouterImpl* parent_;

    /// The event queue
    oasys::MsgQueue<EhsEvent*>* eventq_;

    /// Running as prime or backup flag
    bool prime_mode_;

    /// Flag indicating Forward Link processing enabled
    bool fwdlnk_enabled_;

    /// Flag indicating AOS or LOS
    bool fwdlnk_aos_;

    /// discard all messages until we have gotten the link report
    /// - prevent new incoming bundles being rejected due to an unknown link
    bool have_link_report_;

    /// Log level restriction low level oasys messages to stderr
    int log_level_;

    /// List of all bundles on the node by Bundle ID
    EhsBundleMapWithStats all_bundles_;

    /// List of all bundles on the node by Custody ID
    EhsBundleMap custody_bundles_;

    /// List of bundles destined to the local DTN node that have not been delivered
    EhsBundleMap undelivered_bundles_;

    /// List of bundles with the ECOS Critical bit set
    EhsBundleList critical_bundles_;

    /// List of Bundle IDs that we receive a delivery notice for when
    /// we do not have the bundle in our pending list. The delivered notice
    /// is receied before the bundle received notice.
    DeliveredBundleIDList delivered_bundle_id_list_;

    typedef std::map<uint64_t, EhsBundleMap*> BUNDLES_BY_DEST_MAP;
    typedef BUNDLES_BY_DEST_MAP::iterator BUNDLES_BY_DEST_ITER;
    BUNDLES_BY_DEST_MAP bundles_by_dest_map_;


    /// The router object for the node
    EhsRouter* router_;

    /// Source and Destination white list for bundle acceptance
    EhsSrcDstWildBoolMap accept_bundles_;

    /// Source white list for bundle custody acceptance
    EhsSrcDstWildBoolMap accept_custody_;

    /// Sequence Counter for messages received
    uint64_t last_recv_seq_ctr_;

    /// Sequence Counter for sending messages
    uint64_t send_seq_ctr_;


    /**
     * Type to keep statistics by Source-Dest combo
     */
    typedef std::map<EhsSrcDstKey*, EhsSrcDstKey*, EhsSrcDstKey::mapcompare> SrcDstStatsList;
    typedef std::pair<EhsSrcDstKey*, EhsSrcDstKey*> SrcDstStatsPair;
    typedef SrcDstStatsList::iterator SrcDstStatsIterator;

    SrcDstStatsList src_dst_stats_list_;

    
    /// Statistics structure definition
    struct Stats {
        uint64_t preexisting_bundles_;
        uint64_t received_bundles_;
        uint64_t transmitted_bundles_;
        uint64_t delivered_bundles_;
        uint64_t expired_bundles_;
        uint64_t deleted_bundles_;
        uint64_t injected_bundles_;
        uint64_t rejected_bundles_;
    };

    /// Stats instance
    Stats stats_;

};

} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED

#endif /* _EHS_DTN_NODE_H_ */
