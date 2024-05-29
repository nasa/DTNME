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

#ifndef _TCP_CONVERGENCE_LAYER_V3_H_
#define _TCP_CONVERGENCE_LAYER_V3_H_

#include <third_party/oasys/io/TCPClient.h>
#include <third_party/oasys/io/TCPServer.h>
#include <third_party/oasys/serialize/Serialize.h>

#include "StreamConvergenceLayer.h"

namespace dtn {

class IPAnnounce;

/**
 * The TCP Convergence Layer.
 */
class TCPConvergenceLayerV3 : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t TCPCL_VERSION = 0x03;

    /**
     * Default port used by the tcp cl.
     */
    static const u_int16_t TCPCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    TCPConvergenceLayerV3();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]) override;
    void interface_activate(Interface* iface) override;
    bool interface_down(Interface* iface) override;
    void dump_interface(Interface* iface, oasys::StringBuffer* buf) override;
    void list_interface_opts(oasys::StringBuffer& buf) override;
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class TCPLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        void serialize( oasys::SerializeAction *);

        bool      hexdump_;		///< Log a hexdump of all traffic
        in_addr_t local_addr_;		///< Local address to bind to
        in_addr_t remote_addr_;		///< Peer address used for rcvr-connect
        u_int16_t remote_port_;		///< Peer port used for rcvr-connect
        int recv_bufsize_;	// default: system setting
        int send_bufsize_;	// default: system setting

    protected:
        // See comment in LinkParams for why this is protected
        TCPLinkParams(bool init_defaults);
        friend class TCPConvergenceLayerV3;
    };

    /**
     * Default link parameters.
     */
    static TCPLinkParams default_link_params_;

protected:
    friend class IPAnnounce;

    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]) override;
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp) override;
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf) override;
    void list_link_opts(oasys::StringBuffer& buf) override;
    /// @}
    
    /// @{ Virtual from ConvergenceLayer
    virtual CLInfo* new_link_params() override;
    virtual bool parse_link_params(LinkParams* params,
                                   int argc, const char** argv,
                                   const char** invalidp) override;
    virtual bool parse_nexthop(const LinkRef& link, LinkParams* params) override;
    virtual CLConnection* new_connection(const LinkRef& link,
                                         LinkParams* params) override;
    /// @}

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::TCPServerThread {
    public:
        Listener(TCPConvergenceLayerV3* cl);
        void accepted(int fd, in_addr_t addr, u_int16_t port);

        /// The TCPCL instance
        TCPConvergenceLayerV3* cl_;
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
    class Connection3 : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection3(TCPConvergenceLayerV3* cl, TCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection3(TCPConvergenceLayerV3* cl, TCPLinkParams* params,
                   int fd, in_addr_t addr, u_int16_t port);

        /**
         * Destructor.
         */
        virtual ~Connection3();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a) override;

    protected:
        friend class TCPConvergenceLayerV3;

        /// @{ Virtual from CLConnection
        virtual void connect() override;
        virtual void accept() override;
        virtual void disconnect() override;
        virtual void initialize_pollfds() override;
        virtual void handle_poll_activity() override;
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data() override;
        /// @}
        
        /// Hook for handle_poll_activity to receive data
        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        TCPLinkParams* tcp_lparams()
        {
            TCPLinkParams* ret = dynamic_cast<TCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TCPClient* sock_;	///< The socket
        struct pollfd*	  sock_pollfd_;	///< Poll structure for the socket
    };
};

} // namespace dtn

#endif /* _TCP_CONVERGENCE_LAYER_V3_H_ */
