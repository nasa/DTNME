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

#include <map>
#include <string.h>

#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/thread/SpinLock.h>

#include "EhsBundleRef.h"
#include "EhsBundleTree.h"
#include "EhsEventHandler.h"
#include "EhsLink.h"
#include "EhsSrcDstWildBoolMap.h"

#include "routing/ExternalRouterClientIF.h"

namespace dtn {

class EhsDtnNode;
class EhsExternalRouterImpl;
class EhsLink;
class EhsRouter;


// map of DTN Nodes
typedef std::shared_ptr<EhsDtnNode> SPtr_EhsDtnNode;
typedef std::map<std::string, SPtr_EhsDtnNode> EhsDtnNodeMap;
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
                  public oasys::Thread,
                  public ExternalRouterClientIF
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
     * Stop the thread's processing
     */
    virtual void do_shutdown();

    /**
     * Post events to be processed
     */
    virtual void post_event(EhsEvent* event, bool at_back=true);

    /**
     * Main event handling functions
     */
    virtual void handle_event(EhsEvent* event) override;
    virtual void event_handlers_completed(EhsEvent* event);

    /** 
     ** override pure virtual from ExternalRouterClientIF
     **/
    virtual void send_msg(std::string* msg) override;
 
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
    virtual int bundle_delete_all_orig();

    /**
     * EhsLink initiatedcheck for missed bundles
     */
    virtual uint64_t check_for_missed_bundles(uint64_t ipn_node_id);

    /**
     * Report/Status methods
     */
    virtual void update_statistics(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                                   uint64_t& delivered, uint64_t& rejected,
                                   uint64_t& pending, uint64_t& custody, 
                                   bool& fwdlnk_aos, bool& fwdlnk_enabled);
    virtual void update_statistics2(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                                   uint64_t& delivered, uint64_t& rejected,
                                   uint64_t& pending, uint64_t& custody, uint64_t& expired,
                                   bool& fwdlnk_aos, bool& fwdlnk_enabled);
    virtual void update_statistics3(uint64_t& received, uint64_t& transmitted, uint64_t& transmit_failed,
                                   uint64_t& delivered, uint64_t& rejected,
                                   uint64_t& pending, uint64_t& custody, uint64_t& expired,
                                   bool& fwdlnk_aos, bool& fwdlnk_enabled, 
                                   uint64_t& links_open, uint64_t& num_links);
    virtual void bundle_stats(oasys::StringBuffer* buf);
    virtual void bundle_list(oasys::StringBuffer* buf);
    virtual void link_dump(oasys::StringBuffer* buf);
    virtual void fwdlink_transmit_dump(oasys::StringBuffer* buf);
    virtual void bundle_stats_by_src_dst(int* count, EhsBundleStats** stats);
    virtual void fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats);

    virtual void send_link_add_msg(std::string& link_id, std::string& next_hop, std::string& link_mode,
                                   std::string& cl_name,  LinkParametersVector& params);
    virtual void send_link_del_msg(std::string& link_id);

    virtual void unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf);
    virtual void set_link_statistics(bool enabled);

    /**
     * Accessors
     */
    virtual const std::string* eid()          { return &eid_; }
    virtual const std::string* eid_ping()     { return &eid_ping_; }
    virtual const std::string* eid_ipn()      { return &eid_ipn_; }
    virtual const std::string* eid_ipn_ping() { return &eid_ipn_ping_; }
    virtual const std::string* eid_ipn_dtpc() { return &eid_ipn_dtpc_; }
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
    void run() override;

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
    virtual void handle_cbor_received(EhsCborReceivedEvent* event) override;
    virtual void handle_free_bundle_req(EhsFreeBundleReq* event) override;
//    virtual void handle_delete_bundle_req(EhsDeleteBundleReq* event) override;

    /**
     * Message processing methods
     */
    virtual void process_hello_msg(CborValue& cvElement, uint64_t msg_version);
    virtual void process_alert_msg_v0(CborValue& cvElement);
    virtual void process_bundle_report_v0(CborValue& cvElement);
    virtual void process_bundle_received_v0(CborValue& cvElement);
    virtual void process_bundle_transmitted_v0(CborValue& cvElement);
    virtual void process_bundle_delivered_v0(CborValue& cvElement);
    virtual void process_bundle_expired_v0(CborValue& cvElement);
    virtual void process_bundle_cancelled_v0(CborValue& cvElement);
    virtual void process_custody_timeout_v0(CborValue& cvElement);
    virtual void process_custody_accepted_v0(CborValue& cvElement);
    virtual void process_custody_signal_v0(CborValue& cvElement);
    virtual void process_agg_custody_signal_v0(CborValue& cvElement);


    virtual void finalize_bundle_delivered_event(uint64_t id);

    /**
     * Utility methods
     */
    virtual void route_bundle(EhsBundleRef& bref);
    virtual bool is_local_destination(EhsBundleRef& bref);

    virtual bool accept_custody_before_routing(EhsBundleRef& bref);

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

    /// ping (service number 129 for ISS) EID (IPN scheme) of the DTN Node
    std::string eid_ipn_dtpc_;

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

    /// values provided in the last received hello message
    uint64_t last_hello_bundles_received_ = 0;
    uint64_t last_hello_bundles_pending_ = 0;

};

} // namespace dtn

#endif /* _EHS_DTN_NODE_H_ */
