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

#ifndef _EHS_ROUTER_H_
#define _EHS_ROUTER_H_

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



#include "EhsBundle.h"
#include "EhsBundleTree.h"
#include "EhsEventHandler.h"
#include "EhsLink.h"
#include "EhsSrcDstWildBoolMap.h"
#include "router-custom.h"



namespace dtn {

class EhsDtnNode;

class EhsRouter: public EhsEventHandler,
                 public oasys::Thread
{
public:
    /**
     * Constructor for the different message types
     */
    EhsRouter(EhsDtnNode* parent);

    /**
     * Destructor.
     */
    virtual ~EhsRouter();

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
     * Check to see if okay to accept a bundle
     */
    virtual bool accept_bundle(EhsBundleRef& bref, std::string& link_id, 
                               std::string& remote_addr);
    /**
     * Pass through a message to the EhsExernalRouter to be sent to the DTNME node
     */
    virtual void send_msg(rtrmessage::bpa& msg);

    /**
     * An EhsLink class uses this to return its bundles to be re-routed because it closed
     */
    virtual uint64_t return_all_bundles_to_router(EhsBundlePriorityTree& tree);

    /**
     * Allow Link to request any missed bundles
     */
    virtual uint64_t check_for_missed_bundles(EhsLink* link);
    virtual uint64_t check_dtn_node_for_missed_bundles(uint64_t ipn_node_id);

    /**
     * Determine if the given destination node is sent through the Forward Link
     */
    virtual bool is_fwd_link_destination(uint64_t dest_node);
    
    /**
     * Report/Status methods
     */
    //virtual void stats(oasys::StringBuffer* buf);
    virtual void link_dump(oasys::StringBuffer* buf);
    virtual void fwdlink_transmit_dump(oasys::StringBuffer* buf);
    virtual void unrouted_bundle_stats_by_src_dst(oasys::StringBuffer* buf);
    virtual void set_link_statistics(bool enabled);

    /**
     * Accessors
     */
    virtual bool prime_mode()              { return prime_mode_; }
    virtual bool fwdlnk_enabled()          { return fwdlnk_enabled_; }
    virtual bool fwdlnk_aos()              { return fwdlnk_aos_; }
    virtual bool fwdlnk_force_LOS_while_disabled() { return fwdlnk_force_LOS_while_disabled_; }
    virtual size_t num_unrouted_bundles()  { return unrouted_bundles_.size(); }

    /**
     * Setters
     */
    virtual void set_prime_mode(bool prime_mode);
    virtual void set_fwdlnk_enabled_state(bool state);
    virtual void set_fwdlnk_aos_state(bool state);
    virtual void set_fwdlnk_force_LOS_while_disabled(bool force_los);
    virtual void set_fwdlnk_throttle(uint32_t bps);

    virtual void reconfigure_link(std::string link_id);
    virtual bool get_link_configuration(EhsLink* el);

    virtual void reconfigure_source_priority();
    virtual void reconfigure_dest_priority();

    virtual void fwdlink_transmit_enable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list);
    virtual void fwdlink_transmit_disable(EhsNodeBoolMap& src_list, EhsNodeBoolMap& dst_list);
    virtual void link_enable(EhsLinkCfg* cl);
    virtual void link_disable(std::string& linkid);

    /**
     * Log message methods which format and pass messages back to the application
     */
    virtual void set_log_level(int level);
    virtual void log_msg_va(const std::string& path, oasys::log_level_t level, const char*format, va_list args);

protected:
    /**
     * Thread run method
     */
    void run();

    /**
     * Event handler methods
     */
    virtual void handle_route_bundle_req(EhsRouteBundleReq* event);
    virtual void handle_delete_bundle_req(EhsDeleteBundleReq* event);
    virtual void handle_bpa_received(EhsBpaReceivedEvent* event);
    virtual void handle_bundle_transmitted_event(EhsBundleTransmittedEvent* event);

    /**
     * Message processing methods
     */
    virtual void process_link_report(rtrmessage::bpa* msg);
    virtual void process_link_available_event(rtrmessage::bpa* msg);
    virtual void process_link_opened_event(rtrmessage::bpa* msg);
    virtual void process_link_unavailable_event(rtrmessage::bpa* msg);
    virtual void process_link_busy_event(rtrmessage::bpa* msg);
    virtual void process_link_closed_event(rtrmessage::bpa* msg);

    /**
     * getters
     */
    virtual bool have_link_report() { return have_link_report_; }

    /**
     * Log message method for internal use - passes log info to parent
     */
    virtual void log_msg(oasys::log_level_t level, const char*format, ...);

protected:
    /// Lock for sequential access
    oasys::SpinLock lock_;

    /// Pointer to the parent EhsDtnNode
    EhsDtnNode* parent_;

    /// The event queue
    oasys::MsgQueue<EhsEvent*>* eventq_;

    /// Running as prime or backup flag
    bool prime_mode_;

    /// Log level restriction low level oasys messages to stderr
    int log_level_;

    /// Flag indicating Forward Link processing enabled
    bool fwdlnk_enabled_;

    /// Indication of the state of the space link (AOS or LOS)
    bool fwdlnk_aos_;

    /// Flag indicating whether or not to allow LTP ACKs while disabled
    bool fwdlnk_force_LOS_while_disabled_;

    /// Pointer to the EhsLink object that utilizes the Forward Link transmission
    EhsLink* ehslink_fwd_;

    /// discard all messages until we have gotten the link report
    /// - prevent new incoming bundles being rejected due to an unknown link
    bool have_link_report_;

    /// List of all links by name
    EhsLinkMap all_links_;

    /// Map of BundleMaps - by destination EID with stats ??
    EhsBundleTree unrouted_bundles_;

    /// List of Source/Dest combos that are enbled for transmit over the forward link
    EhsSrcDstWildBoolMap fwdlink_xmt_enabled_;
};

} // namespace dtn

#endif /* defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED) */

#endif // EHSROUTER_ENABLED

#endif /* _EHS_ROUTER_H_ */
