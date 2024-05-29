/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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


#ifndef _EXTERNAL_ROUTER_H_
#define _EXTERNAL_ROUTER_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#include <third_party/meutils/thread/MsgQueue.h>

#include <third_party/oasys/io/UDPClient.h>
#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/util/TokenBucket.h>
#include <third_party/oasys/io/TCPClient.h>
#include <third_party/oasys/io/TCPServer.h>

#include "BundleRouter.h"
#include "IMCRouter.h"
#include "RouteTable.h"
#include "ExternalRouterServerIF.h"
#include <reg/Registration.h>

#define EXTERNAL_ROUTER_SERVICE_TAG "/ext.rtr/*"

namespace dtn {

/**
 * ExternalRouter provides a plug-in interface for third-party
 * routing protocol implementations.
 *
 * Events received from BundleDaemon are serialized into
 * XML messages and UDP multicasted to external bundle router processes.
 * XML actions received on the interface are validated, transformed
 * into events, and placed on the global event queue.
 */
class ExternalRouter : public BundleRouter,
                       public ExternalRouterServerIF {
public:
    /// UDP port for IPC with external routers
    static u_int16_t server_port;

    /// Seconds between hello messages
    static u_int16_t hello_interval;

    /// The network interface to use for communication
    static in_addr_t network_interface_;
    static bool network_interface_configured_;

    ExternalRouter();
    virtual ~ExternalRouter();

    /**
     * Called after all global data structures are set up.
     */
    virtual void initialize() override;

    /**
     * External router clean shutdown
     */
    virtual void shutdown() override;

    /**
     * override the BundleRouter thread run method to basically do nothing
     */
    virtual void run() override;

    /**
     * Synchronous probe indicating whether or not custody of this bundle 
     * should be accepted by the system.
     *
     * The default implementation returns true to match DTNME.9 
     * functionality which did not check with the router.
     * -- External part of this router will issue a request to accept 
     *    custody of the bundle after getting the BundleReceived 
     *    message if it decides it is warranted.
     *
     * @return true if okay to accept custody of the bundle.
     */
    virtual bool accept_custody(Bundle* bundle) override;

    /**
     * Hook to ask the router if the bundle can be deleted.
     */
    bool can_delete_bundle(const BundleRef& bundle) override;
    
    /**
     * seconds a connection should delay after contact up to allow router time
     * to reject the connection before reading bundles
     */
    virtual int delay_after_contact_up()  override { return 3; } 
    
    /**
     * Format the given StringBuffer with static routing info.
     * @param buf buffer to fill with the static routing table
     */
    virtual void get_routing_state(oasys::StringBuffer* buf) override;

    /**
     * Serialize events and UDP multicast to external routers.
     * @param event BundleEvent to process
     */
    virtual void handle_event(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_received(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_transmitted(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_delivered(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_expired(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_free(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_cancelled(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_injected(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_custody_accepted(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_up(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_down(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_created(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_deleted(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_available(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_unavailable(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_attribute_changed(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_attribute_changed(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_new_eid_reachable(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_added(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_removed(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_registration_expired(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_route_add(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_route_del(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_custody_signal(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_external_router_acs(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_custody_timeout(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_report(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_link_attributes_report(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_contact_report(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_report(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_bundle_attributes_report(SPtr_BundleEvent& sptr_event) override;
    virtual void handle_route_report(SPtr_BundleEvent& sptr_event) override;

    virtual void generate_bard_usge_report();


    // for handling ACS and BIBE custody releases on individual bundles
    virtual void handle_custody_released(uint64_t bundleid, bool succeeded, int reason) override;

    /**
     * override pure virtual from ExternalRouterServerIF
     */
    virtual void send_msg(std::string* msg) override;

    virtual std::string lowercase(const char *c_str);

protected:
    /**
     * Add a route entry to the routing table used for IMC bundles handled locally vs externally
     */
    void add_route(RouteEntry *entry);

    /**
     * Remove matching route entry(s) from the routing table. 
     */
    void del_route(const SPtr_EIDPattern& id);

    /**
     * Add a route on the fly when a contact is established if configured to do so.
     */
    void add_nexthop_route(const LinkRef& link);

protected:
    class ModuleServer;
    class HelloTimer;
    class ERRegistration;

    typedef std::unique_ptr<ExternalRouter::ModuleServer> QPtr_ModuleServer;
    typedef std::shared_ptr<ExternalRouter::HelloTimer> SPtr_HelloTimer;


    // XXX This function should really go in ContactEvent
    //     but ExternalRouter needs a less verbose version
    static const char *reason_to_str(int reason);

    /// flag to indicate shutting down
    bool shutting_down_;

    /// UDP server thread
   QPtr_ModuleServer srv_;

    /// The route table
    RouteTable *route_table_;

    /// ExternalRouter registration with the bundle forwarder
    //dzdebug  ERRegistration *reg_;

    /// Hello timer
    SPtr_HelloTimer hello_;

    /// Sequence Counter for sending messages
    uint64_t send_seq_ctr_;

    /// Initialized flag
    bool initialized_;

    /// Send sequentializer
    oasys::SpinLock lock_;

    /// The routing table for IMC bundles handled locally/statically vs externally
    SPtr_RouteTable sptr_route_table_;

    /// Pointer to the instantiation of the IMC Router
    SPtr_IMCRouter sptr_imc_router_;
};

/**
 * Helper class (and thread) that manages communications
 * with external routers
 */
class ExternalRouter::ModuleServer : public oasys::Thread,
                                     public oasys::Logger,
                                     public ExternalRouterServerIF {
public:
    ModuleServer(ExternalRouter* parent);
    virtual ~ModuleServer();
    virtual void do_shutdown();

    /**
     * Post a string to process from the ExternalRouter counterpart
     */
    virtual void post(std::string* event);

    /**
     * Post a string to send to the ExternalRouter counterpart
     */
    virtual void post_to_send(std::string* event);


    /**
     * The main thread loop
     */
    virtual void run() override;

    /**
     * Parse incoming actions and place them on the
     * global event queue
     * @param msg the incoming CBOR encoded message
     */
    int process_action(std::string* msg);
    void process_transmit_bundle_req_msg_v0(CborValue& cvElement);
    void process_link_reconfigure_req_msg_v0(CborValue& cvElement);
    void process_link_close_req_msg_v0(CborValue& cvElement);
    void process_link_add_req_msg_v0(CborValue& cvElement);
    void process_link_del_req_msg_v0(CborValue& cvElement);
    void process_take_custody_req_msg_v0(CborValue& cvElement);
    void process_delete_bundle_req_msg_v0(CborValue& cvElement);
    void process_delete_all_bundles_req_msg_v0(CborValue& cvElement);
    void process_shutdown_req_msg_v0(CborValue& cvElement);
    void process_bard_usage_req_msg_v0(CborValue& cvElement);
    void process_bard_add_quota_req_msg_v0(CborValue& cvElement);
    void process_bard_del_quota_req_msg_v0(CborValue& cvElement);


    /**
     * Parse incoming actions and place them on the
     * global event queue
     * @param payload the incoming XML document payload
     */
    void process_action_xml(const char *payload);

    /**
     * override pure virtual from ExternalRouterServerIF
     */
    virtual void send_msg(std::string* msg) override { (void) msg; }


    /// Message queue for accepting BundleEvents from ExternalRouter
    meutils::MsgQueue< std::string * > me_eventq_;


protected:

    /**
     * Helper class (and thread) that listens on a TCP
     * interface for incoming data.
     */
    /// Helper class to accept incoming TCP connections
    class TcpConnection : public oasys::TCPClient,
                          public oasys::Thread
    {
        public:

            /// Constructor called when a new connection was accepted
            TcpConnection(ModuleServer* parent, int fd,
                       in_addr_t client_addr, u_int16_t client_port);

            /// Destructor
            ~TcpConnection();

            /**
             * External router clean shutdown
             */
            virtual void do_shutdown();

            /**
             * Post a string to send to the ExternalRouter counterpart
             */
            virtual void post_to_send(std::string* event);

            /// setters and getters

        protected:

            /// virtual run method
            void run();

            /**
             * Handler to process an arrived packet.
             */
            void process_data(u_char* bp, size_t len);


        protected:
            class TcpSender : public oasys::Thread,
                              public oasys::Logger {
            public:
                TcpSender(TcpConnection* parent);
                virtual ~TcpSender();
                virtual void run();
                virtual void post(std::string* event);

            protected:
                /// Message queue for accepting BundleEvents from ExternalRouter
                meutils::MsgQueue< std::string * > me_eventq_;

                TcpConnection* parent_;

                oasys::SpinLock eventq_lock_;
                size_t eventq_bytes_ = 0;
                size_t eventq_bytes_max_ = 0;
            };


            virtual void sender_closed ( TcpSender* sender );


            /// Parameters for the connection
            in_addr_t         client_addr_;
            u_int16_t         client_port_;
            ModuleServer*     parent_;

            TcpSender* tcp_sender_;
            oasys::SpinLock lock_;
    };



    class TcpInterface : public oasys::TCPServerThread {

    public:

        TcpInterface(ModuleServer* parent, in_addr_t listen_addr, u_int16_t listen_port);
        void accepted(int fd, in_addr_t addr, u_int16_t port);
   

        /// Destructor
        ~TcpInterface(); 

        /**
         * External router clean shutdown
         */
        virtual void do_shutdown();

        /**
         * Post a string to send to the ExternalRouter counterpart
         */
        virtual void post_to_send(std::string* event);


        /**
         * Notification that a connection has closed
         */
        void client_closed(TcpConnection* cl);

    protected:

        /// @{
        /// Proxy parameters
        in_addr_t listen_addr_;
        u_int16_t listen_port_;
        /// @}

        /// Pointer back to the parent ModuleServer
        ModuleServer* parent_;
        TcpConnection* router_client_;

        oasys::SpinLock lock_;
    };

    typedef std::unique_ptr<TcpInterface> QPtr_TcpInterface;



public:

    /// Means for TCPConnection to inform ModuleServer that it has closed
    void client_closed(TcpConnection* cl);


private:
    ExternalRouter* parent_;

    QPtr_TcpInterface tcp_interface_;
    oasys::SpinLock lock_;

private:

    /// Sequence Counter for messages receivevd
    uint64_t last_recv_seq_ctr_;
};

/**
 * Helper class for ExternalRouter hello messages
 */
class ExternalRouter::HelloTimer : public oasys::SharedTimer {
public:
    HelloTimer(ExternalRouter *router);
    virtual ~HelloTimer();

    virtual void start(uint32_t seconds, SPtr_HelloTimer& sptr);

    virtual void cancel();

    virtual void timeout(const struct timeval &now) override;

protected:
    ExternalRouter *router_;

    oasys::SPtr_Timer sptr_;

    uint32_t seconds_ = 30;
};


#if 0
/**
 * Helper class which registers to receive
 * bundles from remote peers
 */
class ExternalRouter::ERRegistration : public Registration {
public:
    ERRegistration(ExternalRouter *router);

    /**
     * Registration delivery callback function
     */
    void deliver_bundle(Bundle *bundle);

    ExternalRouter *router_;
};
#endif


/**
 * Global shutdown callback function
 */
void external_rtr_shutdown(void *args);

} // namespace dtn

#endif //_EXTERNAL_ROUTER_H_
