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

#ifndef _SERIAL_CONVERGENCE_LAYER_H_
#define _SERIAL_CONVERGENCE_LAYER_H_

#include <oasys/io/TTY.h>
#include <oasys/serialize/Serialize.h>

#include "StreamConvergenceLayer.h"

namespace dtn {

/**
 * The Serial Convergence Layer.
 */
class SerialConvergenceLayer : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t SERIALCL_VERSION = 0x01;

    /**
     * Byte sent on the wire to synchronize the two ends.
     */
    static const u_char SYNC = '.';

    /**
     * Constructor.
     */
    SerialConvergenceLayer();

    /**
     * Tunable link parameter structure.
     */
    class SerialLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

        bool        hexdump_;		///< Log a hexdump of all traffic
        std::string initstr_;		///< String to initialize the tty
        u_int       ispeed_;		///< Input speed on the tty
        u_int       ospeed_;		///< Output speed on the tty
        u_int	    sync_interval_;	///< Interval to send initial sync bits

    protected:
        // See comment in LinkParams for why this is protected
        SerialLinkParams(bool init_defaults);
        friend class SerialConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static SerialLinkParams default_link_params_;

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
         * Constructor for a connection.
         */
        Connection(SerialConvergenceLayer* cl,
                   const LinkRef&          link,
                   SerialLinkParams*       params);

        /**
         * Destructor.
         */
        virtual ~Connection();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

    protected:
        friend class SerialConvergenceLayer;

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_timeout();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data();
        /// @}

        /// Hook for handle_poll_activity to receive data
        void recv_data();

        /// Send a sync byte
        void send_sync();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        SerialLinkParams* serial_lparams()
        {
            SerialLinkParams* ret = dynamic_cast<SerialLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TTY*    tty_;		///< The tty
        struct pollfd* tty_pollfd_;	///< Poll structure for the tty
        bool	       synced_;		///< Whether the SYNC has completed
    };
};

} // namespace dtn

#endif /* _SERIAL_CONVERGENCE_LAYER_H_ */
