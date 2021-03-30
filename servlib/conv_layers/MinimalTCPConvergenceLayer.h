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

#ifndef _MINIMAL_TCP_CONVERGENCE_LAYER_H_
#define _MINIMAL_TCP_CONVERGENCE_LAYER_H_

#include <third_party/oasys/io/TCPClient.h>
#include <third_party/oasys/io/TCPServer.h>
#include <third_party/oasys/serialize/Serialize.h>

#include "bundling/CborUtil.h"
#include "StreamConvergenceLayer.h"

namespace dtn {

/**
 * The STCP Convergence Layer.
 */
class MinimalTCPConvergenceLayer : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t MTCPCL_VERSION = 0x00;

    /**
     * Default port used by the tcp cl.
     */
    static const uint16_t MTCPCL_DEFAULT_PORT = 4558;
    
    /**
     * Constructor.
     */
    MinimalTCPConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    void list_interface_opts(oasys::StringBuffer& buf) override;
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class MTCPLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        void serialize( oasys::SerializeAction *);

        bool      hexdump_ = false;                    ///< Log a hexdump of all traffic
        in_addr_t local_addr_ = INADDR_ANY;            ///< Local address to bind to
        uint16_t  local_port_ = MTCPCL_DEFAULT_PORT;   ///< Local port to bind to
        in_addr_t remote_addr_ = INADDR_NONE;          ///< Peer address used for rcvr-connect
        uint16_t  remote_port_ = MTCPCL_DEFAULT_PORT;  ///< Peer port used for rcvr-connect

    protected:
        // See comment in LinkParams for why this is protected
        MTCPLinkParams(bool init_defaults);
        MTCPLinkParams(): MTCPLinkParams(true) {}
        friend class MinimalTCPConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    MTCPLinkParams default_link_params_;

protected:
    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    void list_link_opts(oasys::StringBuffer& buf) override;
    /// @}
    
    /// @{ Virtual from ConvergenceLayer
    virtual CLInfo* new_link_params();
    virtual bool parse_link_params(LinkParams* params,
                                   int argc, const char** argv,
                                   const char** invalidp);
    virtual bool parse_nexthop(const LinkRef& link, LinkParams* params);
    virtual CLConnection* new_connection(const LinkRef& link,
                                         LinkParams* params);
    /// @}

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::TCPServerThread {
    public:
        Listener(MinimalTCPConvergenceLayer* cl, MTCPLinkParams* params,
                 const std::string& iface_name);
        virtual ~Listener() {}
        virtual bool is_initialized() { return is_initialized_; }
        void accepted(int fd, in_addr_t addr, uint16_t port) override;

    protected:
        virtual void init_listener();

    protected:
        /// The STCPCL instance
        MinimalTCPConvergenceLayer* cl_ = nullptr;
        MTCPLinkParams* params_ = nullptr;
        bool is_initialized_ = false;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that to deal with NAT, the side
     * which does the active connect is not necessarily the sender.
     */
    class Connection : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection(MinimalTCPConvergenceLayer* cl, MTCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection(MinimalTCPConvergenceLayer* cl, MTCPLinkParams* params,
                   int fd, in_addr_t addr, uint16_t port);

        /**
         * Destructor.
         */
        virtual ~Connection();

        /**
         * Wrapper for the CLConnection::run method to allow setting the name of the thread
         */
        virtual void run() override;

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

    protected:
        friend class MinimalTCPConvergenceLayer;

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void accept();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        virtual void send_data();
        virtual void process_data();
        virtual void send_keepalive();

        virtual void initiate_contact();
        virtual void break_contact(ContactEvent::reason_t reason);
        virtual void handle_contact_initiation();
        virtual bool handle_data_segment(uint8_t flags);
        /// @}
        
        /// Hook for handle_poll_activity to receive data
        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        MTCPLinkParams* mtcp_lparams()
        {
            MTCPLinkParams* ret = dynamic_cast<MTCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
    protected:
        oasys::TCPClient* sock_;    ///< The socket
        struct pollfd*      sock_pollfd_;    ///< Poll structure for the socket

        CborUtil cborutil_;

    private:
        virtual bool send_next_segment(InFlightBundle* inflight);
    };
};

} // namespace dtn

#endif /* _MINIMAL_TCP_CONVERGENCE_LAYER_H_ */
