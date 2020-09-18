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

#ifndef _STCP_CONVERGENCE_LAYER_H_
#define _STCP_CONVERGENCE_LAYER_H_

#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>
#include <oasys/serialize/Serialize.h>

#include "StreamConvergenceLayer.h"

namespace dtn {

/**
 * The STCP Convergence Layer.
 */
class STCPConvergenceLayer : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t STCPCL_VERSION = 0x00;

    /**
     * Default port used by the tcp cl.
     */
    static const u_int16_t STCPCL_DEFAULT_PORT = 4557;
    
    /**
     * Constructor.
     */
    STCPConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class STCPLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        void serialize( oasys::SerializeAction *);

        bool      hexdump_;		///< Log a hexdump of all traffic
        in_addr_t local_addr_;		///< Local address to bind to
        in_addr_t remote_addr_;		///< Peer address used for rcvr-connect
        u_int16_t remote_port_;		///< Peer port used for rcvr-connect

    protected:
        // See comment in LinkParams for why this is protected
        STCPLinkParams(bool init_defaults);
        friend class STCPConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static STCPLinkParams default_link_params_;

protected:
    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
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
        Listener(STCPConvergenceLayer* cl);
        void accepted(int fd, in_addr_t addr, u_int16_t port);

        /// The STCPCL instance
        STCPConvergenceLayer* cl_;
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
        Connection(STCPConvergenceLayer* cl, STCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection(STCPConvergenceLayer* cl, STCPLinkParams* params,
                   int fd, in_addr_t addr, u_int16_t port);

        /**
         * Destructor.
         */
        virtual ~Connection();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

    protected:
        friend class STCPConvergenceLayer;

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
        virtual void handle_contact_initiation();
        virtual bool handle_data_segment(u_int8_t flags);
        /// @}
        
        /// Hook for handle_poll_activity to receive data
        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        STCPLinkParams* stcp_lparams()
        {
            STCPLinkParams* ret = dynamic_cast<STCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TCPClient* sock_;	///< The socket
        struct pollfd*	  sock_pollfd_;	///< Poll structure for the socket

    private:
        virtual bool send_next_segment(InFlightBundle* inflight);
    };
};

} // namespace dtn

#endif /* _STCP_CONVERGENCE_LAYER_H_ */
