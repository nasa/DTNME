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

#ifndef _EHS_EXTERNAL_ROUTER_IMPL_H_
#define _EHS_EXTERNAL_ROUTER_IMPL_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <oasys/io/UDPClient.h>
#include <oasys/io/TCPClient.h>
#include <oasys/serialize/XercesXMLSerialize.h>
#include <oasys/thread/MsgQueue.h>

#include "EhsDtnNode.h"
#include "EhsSrcDstWildBoolMap.h"
#include "EhsLink.h"
#include "router-custom.h"

namespace dtn {


typedef void (*LOG_CALLBACK_FUNC)(const std::string& path, int level, const std::string& msg);


/**
 * EhsExternalRouterImpl is an ExternalRouter implementation specific to MSFC and ISS
 */
class EhsExternalRouterImpl : public oasys::Thread,
                              public oasys::UDPClient {
public:
    EhsExternalRouterImpl(LOG_CALLBACK_FUNC log_msg, int log_level=4, bool oasys_app=false);
    virtual ~EhsExternalRouterImpl();

    /**
     * The main thread loop
     */
    virtual void run();
    virtual void run_multicast();
    virtual void run_tcp();

    /**
     * Issue shutdown request to the DTNME server(s)
     */
    virtual bool shutdown_server();

    /**
     * String parsing configuration methods 
     * NOTE: these can be called before start()
     */
    virtual bool configure_use_tcp_interface(std::string& val);
    virtual bool configure_mc_address(std::string& val);
    virtual bool configure_mc_port(std::string& val);
    virtual bool configure_network_interface(std::string& val);
    virtual bool configure_schema_file(std::string& val);

    virtual bool configure_link_throttle(std::string& link_id, uint32_t throttle_bps);

    virtual bool configure_accept_custody(std::string& val);

    virtual bool configure_forward_link(std::string& val);
    virtual bool configure_fwdlink_transmit_enable(std::string& val);
    virtual bool configure_fwdlink_transmit_disable(std::string& val);
    virtual void configure_fwdlnk_allow_ltp_acks_while_disabled(bool allowed);
    virtual bool configure_link_enable(std::string& val);
    virtual bool configure_link_disable(std::string& val);
    virtual bool configure_max_expiration_fwd(std::string& val);
    virtual bool configure_max_expiration_rtn(std::string& val);

    /**
     * Configure priority for a node. Valid range is 0 to 999 
     * with 999 being the highest priority. Default priority is 500 
     * so you can raise or lower a node's priority in relation to 
     * all other nodes. 
     */
    virtual bool configure_source_node_priority(std::string& val);
    virtual bool configure_source_node_priority(uint64_t node_id, int priority);
    virtual bool configure_dest_node_priority(std::string& val);
    virtual bool configure_dest_node_priority(uint64_t node_id, int priority);

    /**
     * Alternative configuration methods
     */


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
     * Link State control methods
     */
    virtual int set_fwdlnk_enabled_state(bool state, uint32_t timeout_ms);
    virtual int set_fwdlnk_aos_state(bool state, uint32_t timeout_ms);
    virtual int set_fwdlnk_throttle(uint32_t bps, uint32_t timeout_ms);


    /**
     * Report/Status methods
     */
    virtual int num_dtn_nodes();
    virtual const char* update_statistics();
    virtual void bundle_stats(oasys::StringBuffer* buf);
    virtual void bundle_list(oasys::StringBuffer* buf);
    virtual void link_dump(oasys::StringBuffer* buf);
    virtual void fwdlink_transmit_dump(oasys::StringBuffer* buf);
    virtual void unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf);
    virtual void set_link_statistics(bool enabled);

    virtual void bundle_stats_by_src_dst(int* count, EhsBundleStats** stats);
    virtual void bundle_stats_by_src_dst_free(int count, EhsBundleStats** stats);

    virtual void fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats);
    virtual void fwdlink_interval_stats_free(int count, EhsFwdLinkIntervalStats** stats);

    /// Max bytes sent/recv in one second
    uint64_t max_bytes_sent();
    uint64_t max_bytes_recv()   { return max_bytes_recv_; }


    /**
     * Parse incoming actions and place them on the
     * global event queue
     * @param payload the incoming XML document payload
     */
    void process_action(const char *payload);

    /**
     * Send a message to the DTNME ExternalRouterImpl interface
     */
    virtual void send_msg(rtrmessage::bpa &message, const std::string* server_eid);
    virtual void send_tcp_msg(std::string* msg);

    /**
     * Internal methods to reconfigure Configure the EhsLink
     */
    virtual bool get_link_configuration(EhsLink* el);
    virtual void get_accept_custody_list(EhsSrcDstWildBoolMap& accept_custody);
    virtual void get_fwdlink_transmit_enabled_list(EhsSrcDstWildBoolMap& enabled_list);

    virtual void get_source_node_priority(NodePriorityMap& pmap);
    virtual void get_dest_node_priority(NodePriorityMap& pmap);

    /**
     * Log message methods which format and pass messages back to the application
     */
    virtual void set_log_level(int level);
    virtual void log_msg(const std::string& path, oasys::log_level_t level, const char* format, ...);
    virtual void log_msg(const std::string& path, oasys::log_level_t level, std::string& msg);
    virtual void log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args);

protected:
    /**
     * Log message method for internal use
     */
    virtual void log_msg(oasys::log_level_t level, const char* format, ...);

protected:
    virtual void init();

    virtual bool open_socket();

    virtual EhsDtnNode* get_dtn_node(std::string eid, std::string eid_ipn, std::string alert);



    class TcpSender;

    /**
     * callback when tcp sender exits
     */
    virtual void tcp_sender_closed(TcpSender* tcp_sender);

private:
//    Link::link_type_t convert_link_type(rtrmessage::linkTypeType type);
//    Bundle::priority_values_t convert_priority(rtrmessage::bundlePriorityType);
//    ForwardingInfo::action_t convert_fwd_action(rtrmessage::bundleForwardActionType);


protected:
    class Sender;

    /// Lock to serialize access between threads
    oasys::SpinLock lock_;

    /// Lock to serialize returning log messages to parent application
    oasys::SpinLock log_lock_;

    /// Flag indicating Forward Link processing enabled
    bool fwdlnk_enabled_;

    /// Flag indicating AOS or LOS
    bool fwdlnk_aos_;

    /// Flag indicating whether or not to allow LTP ACKs while disabled
    bool fwdlnk_force_LOS_while_disabled_;

    /// Separate thread to handle sending messages back to the DTN node(s)
    Sender* sender_;
    TcpSender* tcp_sender_;

    /// Message queue for accepting transmit requests 
    oasys::MsgQueue< std::string * > *eventq;

    /// Xerces XML validating parser for incoming messages
    oasys::XercesXMLUnmarshal *parser_;

    /// schema to use in the ExternalRouter communications
    std::string schema_;

    /// Is server validation required?
    bool server_validation_;

    /// Is client validation required?
    bool client_validation_;

    /// map of DTN nodes by EID
    EhsDtnNodeMap dtn_node_map_;

    /// Pointer to the log message function supplied by the parent application
    LOG_CALLBACK_FUNC log_callback_;

    /// Log level restriction low level oasys messages to stderr
    int log_level_;

    /// Flag indicating parent is an oasys application (Log already initialized)
    bool oasys_app_;


    /// List of Link configurations
    EhsLinkCfgMap link_configs_;

    /// Criteria for accepting custody of bundles
    EhsSrcDstWildBoolMap accept_custody_;

    /// Criteria for accepting bundles
    EhsSrcDstWildBoolMap fwdlink_xmt_enabled_;

    /// Max allowed Bundle Expiration Times (TTL)
    uint64_t max_expiration_fwd_;
    uint64_t max_expiration_rtn_;

    /// Configurable priority for source nodes
    NodePriorityMap src_priority_;

    /// Configurable priority for destination nodes
    NodePriorityMap dst_priority_;

    /// Gather stats on bytes received per second
    oasys::Time recv_timer_;
    uint64_t bytes_recv_;
    uint64_t max_bytes_recv_;

    /// Sequence Counter for outgoing messages
    uint64_t send_seq_ctr_;


    /// Configure to use TCP instead of multicast
    bool use_tcp_interface_;
    oasys::TCPClient* tcp_client_;
};

/**
 * Helper class (and thread) that manages communications
 * with external routers
 */
class EhsExternalRouterImpl::Sender: public oasys::Thread,
                                     public oasys::UDPClient {
public:
    Sender(EhsExternalRouterImpl* parent, 
           oasys::IPSocket::ip_socket_params& params,
           in_addr_t local_addr,
           in_addr_t remote_addr, uint16_t remote_port);

    virtual ~Sender();

    /**
     * The main thread loop
     */
    virtual void run();

    /**
     * Parse incoming actions and place them on the
     * global event queue
     * @param payload the incoming XML document payload
     */
    void process_action(const char *payload);

    /// Message queue for accepting BundleEvents from ExternalRouter
    oasys::MsgQueue< std::string * > *eventq;

    oasys::SpinLock *lock_;

    /// Max bytes sent in one second
    uint64_t max_bytes_sent()   { return max_bytes_sent_; }

private:
    /// Pointer to the parent object
    EhsExternalRouterImpl* parent_;

    /// Rate Limiting TokenBucket
    oasys::TokenBucket bucket_;

    /// Gather stats on bytes sent per second
    oasys::Time transmit_timer_;
    uint64_t bytes_sent_;
    uint64_t max_bytes_sent_;
};

class EhsExternalRouterImpl::TcpSender : public oasys::Thread,
                                         public oasys::Logger {
public:
    TcpSender(EhsExternalRouterImpl* parent);
    virtual ~TcpSender();
    virtual void run();
    virtual void post(std::string* event);

    virtual uint64_t max_bytes_sent();

protected:
    /// Message queue for accepting BundleEvents from ExternalRouter
    oasys::MsgQueue< std::string * > *eventq_;

    EhsExternalRouterImpl* parent_;
};

} // namespace dtn

#endif // XERCES_C_ENABLED && EHS_DP_ENABLED

#endif // EHSROUTER_ENABLED

#endif //_EHS_EXTERNAL_ROUTER_IMPL_H_
