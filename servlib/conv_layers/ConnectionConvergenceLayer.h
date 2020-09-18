/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _CONNECTION_CONVERGENCE_LAYER_H_
#define _CONNECTION_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"

namespace dtn {

class CLConnection;

/**
 * All convergence layers that maintain a connection (i.e. a TCP
 * socket) to next hop peers derive from this base class. As such, it
 * manages all communication to/from the main bundle daemon thread,
 * handles the main thread of control for each connection, and
 * dispatches to the specific CL implementation to handle the actual
 * wire protocol.
 
 * The design is as follows:
 *
 * Open links contain a Connection class stored in the Contact's
 * cl_info slot. The lifetime of this object is one-to-one with the
 * duration of the Contact object. The Connection class is a thread
 * that contains a MsgQueue for commands to be sent from the bundle
 * daemon. The commands are SEND_BUNDLE, CANCEL_BUNDLE, and
 * BREAK_CONTACT. When in an idle state, the thread blocks on this
 * queue as well as the socket or other connection object so it can be
 * notified of events coming from either the daemon or the peer node.
 *
 * To enable backpressure, each connection has a maximum queue depth
 * for bundles that have been pushed onto the queue but have not yet
 * been sent or registered as in-flight by the CL. The state of the
 * link is set to BUSY when this limit is reached, but is re-set to
 * AVAILABLE if By default, there is no hard limit on the number of
 * bundles that can be in-flight, instead the limit is determined by
 * the capacity of the underlying link.
 *
 * The hardest case to handle is how to close a contact, as there is a
 * race condition between the underlying connection breaking and the
 * higher layers determining that the link should be closed. If the
 * underlying link breaks due to a timeout or goes idle for an on
 * demand link, a ContactDownEvent is posted and the thread
 * terminates, setting the is_stopped() bit in the thread class. In
 * response to this event, the daemon will call the close_contact
 * method. In this case, the connection thread has already terminated
 * so it is cleaned up when the Contact object goes away.
 *
 * If the link is closed by the daemon thread due to user
 * configuration or a scheduled link's open time elapsing, then
 * close_contact will be called while the connection is still open.
 * The connection thread is informed by sending it a BREAK_CONTACT
 * command. Reception of this command closes the connection and
 * terminates, setting the is_stopped() bit when it is done. All this
 * logic is handled by the break_contact method in the Connection
 * class.
 *
 * Finally, for bidirectional protocols, opportunistic links can be
 * created in response to new connections arriving from a peer node.
 */
class ConnectionConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    ConnectionConvergenceLayer(const char* logpath, const char* cl_name);

    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    void delete_link(const LinkRef& link);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    bool reconfigure_link(const LinkRef& link, int argc, const char* argv[]);
    bool open_contact(const ContactRef& contact);
    bool close_contact(const ContactRef& contact);
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);
    void cancel_bundle(const LinkRef& link, const BundleRef& bundle);
    /// @}

    /**
     * Tunable parameter structure stored in each Link's CLInfo slot.
     * Other CL-specific parameters are handled by deriving from this
     * class.
     */
    class LinkParams : public CLInfo {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *a);

        bool reactive_frag_enabled_;	///< Is reactive fragmentation enabled
        u_int sendbuf_len_;		///< Buffer size for sending data
        u_int recvbuf_len_;		///< Buffer size for receiving data
        u_int data_timeout_;		///< Msecs to wait for data arrival
        
        u_int test_read_delay_;		///< Msecs to sleep between read calls
        u_int test_write_delay_;	///< Msecs to sleep between write calls
        u_int test_recv_delay_;		///< Msecs to sleep before recv evt

        u_int test_read_limit_;		///< Max amount to read from the channel
        u_int test_write_limit_;	///< Max amount to write to the channel

    protected:
        // The only time this constructor should be called is to
        // initialize the default parameters. All other cases (i.e.
        // derivative parameter classes) should use a copy constructor
        // to grab the default settings.
        LinkParams(bool init_defaults);
    };

    /**
     * Parse and validate the nexthop address for the given link.
     */
    virtual bool parse_nexthop(const LinkRef& link, LinkParams* params) = 0;

protected:
    /**
     * Parse the link parameters, returning true iff the args are
     * valid for the given nexthop address.
     */
    virtual bool parse_link_params(LinkParams* params,
                                   int argc, const char** argv,
                                   const char** invalidp);
    
    /**
     * After the link parameters are parsed, do any initialization of
     * the link that's necessary before starting up a connection.
     */
    virtual bool finish_init_link(const LinkRef& link, LinkParams* params);
    
    /**
     * Create a new CL-specific connection object.
     */
    virtual CLConnection* new_connection(const LinkRef& link,
                                         LinkParams* params) = 0;

};

} // namespace dtn

#endif /* _CONNECTION_CONVERGENCE_LAYER_H_ */
