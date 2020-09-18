/*
 *    Copyright 2007-2010 Darren Long, darren.long@mac.com
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

#ifndef _AX25CM_CONVERGENCE_LAYER_H_
#define _AX25CM_CONVERGENCE_LAYER_H_

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

#include <oasys/ax25/AX25ConnectedModeClient.h>
#include <oasys/ax25/AX25ConnectedModeServer.h>
#include <oasys/serialize/Serialize.h>

#include "SeqpacketConvergenceLayer.h"

namespace dtn {

class AX25Announce;

/**
 * The AX25 Connected Mode (SOCK_SEQPACKET) Convergence Layer.
 */
class AX25CMConvergenceLayer : public SeqpacketConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    // follow TCPCL version, as basis of shim protocol
    static const u_int8_t AX25CMCL_VERSION = 0x03; 
    
    /**
     * Constructor.
     */
    AX25CMConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class AX25CMLinkParams : public SeqpacketLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

        bool        hexdump_;           ///< Log a hexdump of all traffic
        std::string local_call_;        ///< Local callsign to bind to
        std::string remote_call_;       ///< Peer callsign
        std::string digipeater_;
        std::string axport_;            ///< Local axport to bind or connect


    protected:
        // See comment in LinkParams for why this is protected
        AX25CMLinkParams(bool init_defaults);
        friend class AX25CMConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static AX25CMLinkParams default_link_params_;

protected:
    friend class AX25Announce;

    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    /// @}
    
    /// @{ Virtual from ConnectionConvergenceLayer
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
    class Listener : public CLInfo, public oasys::AX25ConnectedModeServerThread {
    public:
        Listener(AX25CMConvergenceLayer* cl);
        void accepted(int fd, const std::string& addr);

        /// The AX25CMCL instance
        AX25CMConvergenceLayer* cl_;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that the side which does the active
     * connect is not necessarily the sender.
     */
    class Connection : public SeqpacketConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection(AX25CMConvergenceLayer* cl, AX25CMLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection(AX25CMConvergenceLayer* cl, AX25CMLinkParams* params,
                   int fd, const std::string& local_call, 
                   const std::string& addr, 
                   const std::string& axport);

        /**
         * Destructor.
         */
        virtual ~Connection();

    protected:
        friend class AX25CMConvergenceLayer;

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void accept();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from SeqpacketConvergenceLayer::Connection
        void send_data();
        void process_data();
        
        /// @}
        
        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        AX25CMLinkParams* ax25cm_lparams()
        {
            AX25CMLinkParams* ret = dynamic_cast<AX25CMLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::AX25ConnectedModeClient* sock_;  ///< The socket
        struct pollfd*    sock_pollfd_; ///< Poll structure for the socket
    };
};


/**
 * Utility class
 */
class AX25ConvergenceLayerUtils {
public:
    /**
     * Parse a next hop address specification of the form
     * <host>[:<port>?].
     *
     * @return true if the conversion was successful, false
     */
    static bool parse_nexthop(const char* logpath, const char* nexthop,
                              std::string* local_call,
                              std::string* remote_call, 
                              std::string* digipeater,
                              std::string* axport);
};


} // namespace dtn

#endif /* #ifdef OASYS_AX25_ENABLED  */
#endif /* _AX25CM_CONVERGENCE_LAYER_H_ */
