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

#ifndef _DTPC_DAEMON_H_
#define _DTPC_DAEMON_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef DTPC_ENABLED

#include <oasys/compat/inttypes.h>
#include <oasys/debug/Log.h>
#include <oasys/tclcmd/IdleTclExit.h>
#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>
#include <oasys/thread/SpinLock.h>

#include "bundling/BundleEvent.h"
#include "bundling/BundleEventHandler.h"

#include "DtpcDataPduCollector.h"
#include "DtpcPayloadAggregator.h"
#include "DtpcProtocolDataUnit.h"
#include "DtpcRegistration.h"

namespace dtn {

/**
 * Typedefs for a map of DTPC Topic ID to DtpcRegistrations
 */
typedef std::map<u_int32_t, DtpcRegistration*>         DtpcTopicRegistrationMap;
typedef std::pair<u_int32_t, DtpcRegistration*>        DtpcTopicRegistrationPair;
typedef DtpcTopicRegistrationMap::iterator             DtpcTopicRegistrationIterator;
typedef std::pair<DtpcTopicRegistrationIterator, bool> DtpcTopicRegistrationInsertResult;


class DtpcReceiver;


/**
 * Class that handles the basic event / action mechanism. All events
 * are queued and then forwarded to the active router module. The
 * router then responds by calling various functions on the
 * BundleActions class that it is given, which in turn effect all the
 * operations.
 */
class DtpcDaemon : public oasys::Singleton<DtpcDaemon, false>,
                   public BundleEventHandler,
                   public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    DtpcDaemon();

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~DtpcDaemon();

    /**
     * Virtual initialization function, possibly overridden by a 
     * future simulator
     */
    virtual void do_init();

    /**
     * Boot time initializer.
     */
    static void init();
    
    /**
     * Free up all of the objects allocated
     */
    virtual void clean_up();

    /**
     * Return the number of events currently waiting for processing.
     * This may be overridden in a future simulator
     */
    virtual size_t event_queue_size()
    {
    	return eventq_->size();
    }

    /**
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */
    static void post(BundleEvent* event);
 
    /**
     * Queues the event at the head of the queue for processing by the
     * daemon thread.
     */
    static void post_at_head(BundleEvent* event);
    
    /**
     * Post the given event and wait for it to be processed by the
     * daemon thread or for the given timeout to elapse.
     */
    static bool post_and_wait(BundleEvent* event,
                              oasys::Notifier* notifier,
                              int timeout = -1, bool at_back = true);
    
   /**
    * Virtual post_event function, overridden by the Node class in
     * the simulator to use a modified event queue.
     */
    virtual void post_event(BundleEvent* event, bool at_back = true);

    /**
     * Format the given StringBuffer with the current 
     * statistics.
     */
    virtual void get_stats(oasys::StringBuffer* buf);

    /**
     * Format the given StringBuffer with the current internal
     * statistics value.
     */
    virtual void get_daemon_stats(oasys::StringBuffer* buf);

    /**
     * Reset all internal stats.
     */
    virtual void reset_stats();

    /**
     * Return the local endpoint identifier.
     */
    virtual const EndpointID& local_eid() { return local_eid_; }

    /**
     * Return the local IPN endpoint identifier.
     */
    virtual const EndpointID& local_eid_ipn() { return local_eid_ipn_; }

    /**
     * Set the local endpoint id.
     */
    virtual void set_local_eid(const char* eid_str);

    /**
     * Set the local IPN endpoint id.
     */
    virtual void set_local_eid_ipn(const char* eid_str);

    /**
     * Return the Topic Registration Map
     */
    // needed?  DtpcTopicRegistrationMap* topic_reg_map() { return topic_reg_map_; }A

    /**
     * General daemon parameters
     */
    struct Params {
        /// Default constructor
        Params();
        
        /// Require predefined topics or allow new ones on the fly
        bool require_predefined_topics_;

        /// Restrict sending a topic to the registered client
        bool restrict_send_to_registered_client_;

        /// IPN DTPC Receive Service Number
        u_int64_t ipn_receive_service_number;

        /// IPN DTPC Transmit Service Number (ION implementation)
        u_int64_t ipn_transmit_service_number;
    };

    static Params params_;

    /**
     * Accessor for the DtpcDaemon's shutdown status
     */
    static bool shutting_down()
    {
     	return shutting_down_;
    }

    /**
     * Main event handling function.
     */
    virtual void handle_event(BundleEvent* event);
    virtual void handle_event(BundleEvent* event, bool closeTransaction);

protected:

    /**
     * Initialize and load from the datastore
     */
    virtual void load_profiles();
    virtual void load_topics();
    virtual void load_payload_aggregators();
    virtual void load_data_pdu_collectors();
        
    /**
     * Main thread function that dispatches events.
     */
    void run();

    /// @{
    /**
     * Event type specific handlers.
     */
    virtual void handle_dtpc_topic_registration(DtpcTopicRegistrationEvent* event);
    virtual void handle_dtpc_topic_unregistration(DtpcTopicUnregistrationEvent* event);
    virtual void handle_dtpc_send_data_item(DtpcSendDataItemEvent* event);
    virtual void handle_dtpc_payload_aggregation_timer_expired(DtpcPayloadAggregationTimerExpiredEvent* event);
    virtual void handle_dtpc_transmitted_event(DtpcPduTransmittedEvent* event);
    virtual void handle_dtpc_delete_request(DtpcPduDeleteRequest* event);
    virtual void handle_dtpc_retransmit_timer_expired(DtpcRetransmitTimerExpiredEvent* event);
    virtual void handle_dtpc_ack_received_event(DtpcAckReceivedEvent* event);
    virtual void handle_dtpc_data_received_event(DtpcDataReceivedEvent* event);
    virtual void handle_dtpc_deliver_pdu_event(DtpcDeliverPduTimerExpiredEvent* event);
    virtual void handle_dtpc_topic_expiration_check(DtpcTopicExpirationCheckEvent* event);
    virtual void handle_dtpc_elision_func_response(DtpcElisionFuncResponse* event);
    ///@}

    /// @{
    virtual void event_handlers_completed(BundleEvent* event);
    /// @}

    /// The event queue
    oasys::MsgQueue<BundleEvent*>* eventq_;

    /// The default endpoint id for reaching this daemon, used for
    /// bundle status reports, routing, etc.
    EndpointID local_eid_;

    /// The IPN endpoint id used to take custody of bundles destined to an IPN node
    EndpointID local_eid_ipn_;

    /// Mapping of topics to registrations
    DtpcTopicRegistrationMap topic_reg_map_;

    /// Mapping of Destination/Profile to the Payload Aggregator
    DtpcPayloadAggregatorMap payload_agg_map_;

    /// Map of PDUs pending acknowledgement
    DtpcProtocolDataUnitMap unacked_pdu_list_;

    /// Mapping of Destination/Profile to the Data PDU Collector
    DtpcDataPduCollectorMap pdu_collector_map_;

    /// DTPC Receiver thread
    DtpcReceiver* dtpc_receiver;

    /// Statistics structure definition
    struct Stats {
        u_int32_t events_processed_;
    };

    /// Stats instance
    Stats stats_;

    // indicator that a DtpcDaemon shutdown is in progress
    static bool shutting_down_;

    /// Time value when the last event was handled
    oasys::Time last_event_;
};

} // namespace dtn

#endif // DTPC_ENABLED

#endif /* _DTPC_DAEMON_H_ */
