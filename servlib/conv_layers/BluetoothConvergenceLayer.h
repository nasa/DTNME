/*
 *    Copyright 2006 Baylor University
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

#ifndef _BLUETOOTH_CONVERGENCE_LAYER_
#define _BLUETOOTH_CONVERGENCE_LAYER_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <errno.h>
extern int errno;

#include <oasys/bluez/BluetoothInquiry.h>
#include <oasys/bluez/RFCOMMClient.h>
#include <oasys/bluez/RFCOMMServer.h>

#include "ConnectionConvergenceLayer.h"
#include "StreamConvergenceLayer.h"

namespace dtn {

class BluetoothAnnounce;

/**
 * The Bluetooth Convergence Layer.
 */
class BluetoothConvergenceLayer : public StreamConvergenceLayer {
public:

    /**
     * Current protocol version.
     */
    static const u_int8_t BTCL_VERSION = 0x3; // parallels TCPCL

    /**
     * Default RFCOMM channel used by BTCL
     */
    static const u_int8_t BTCL_DEFAULT_CHANNEL = 10;

    /**
     * Constructor.
     */
    BluetoothConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    /// @}

    /**
     * Tunable link parameter structure
     */
    class BluetoothLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction*a );

        bdaddr_t local_addr_;  ///< Local address to bind to
        bdaddr_t remote_addr_; ///< Remote address to bind to
        u_int8_t channel_;     ///< default channel
    protected:
        // See comment in LinkParams for why this is protected
        BluetoothLinkParams(bool init_defaults);
        friend class BluetoothConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static BluetoothLinkParams default_link_params_;

protected:
    friend class BluetoothAnnounce;

    /// @{ virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    /// @}

    /// @{ virtual from ConvergenceLayer
    virtual CLInfo* new_link_params();
    virtual bool parse_link_params(LinkParams* params, int argc,
                                   const char** argv,
                                   const char** invalidp);
    virtual bool parse_nexthop(const LinkRef& link, LinkParams* params);
    virtual CLConnection* new_connection(const LinkRef& link,
                                         LinkParams* params);
    /// @}

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::RFCOMMServerThread {
    public:
        Listener(BluetoothConvergenceLayer* cl);
        void accepted(int fd, bdaddr_t addr, u_int8_t channel);

        /// The BTCL instance
        BluetoothConvergenceLayer* cl_;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     */
    class Connection : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection(BluetoothConvergenceLayer* cl,
                   BluetoothLinkParams* params);

        /**
         * Constructor for passive accept side of a connection. 
         */
        Connection(BluetoothConvergenceLayer* cl,
                   BluetoothLinkParams* params,
                   int fd, bdaddr_t addr, u_int8_t channel);

        virtual ~Connection();

    protected:

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void accept();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data();
        /// @}

        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        BluetoothLinkParams* bt_lparams()
        {
            BluetoothLinkParams* ret =
                dynamic_cast<BluetoothLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }

        oasys::RFCOMMClient*  sock_;        ///< The socket
        struct pollfd*        sock_pollfd_; ///< Poll structure for the socket 
    };

}; // BluetoothConvergenceLayer

} // namespace dtn

#endif // OASYS_BLUETOOTH_ENABLED
#endif // _BLUETOOTH_CONVERGENCE_LAYER_
