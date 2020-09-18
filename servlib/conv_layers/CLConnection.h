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

#ifndef _CLCONNECTION_H_
#define _CLCONNECTION_H_

#include <list>
#include <oasys/debug/Log.h>
#include <oasys/thread/Atomic.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/SparseBitmap.h>
#include <oasys/util/StreamBuffer.h>

#include "ConnectionConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"

namespace dtn {

/**
 * Helper class (and thread) that manages an established connection
 * with a peer daemon.
 */
class CLConnection : public CLInfo,
                     public oasys::Thread,
                     public oasys::Logger {
public:
    friend class ConnectionConvergenceLayer;
    typedef ConnectionConvergenceLayer::LinkParams LinkParams;
    
    /**
     * Constructor.
     */
    CLConnection(const char* classname,
                 const char* logpath,
                 ConnectionConvergenceLayer* cl,
                 LinkParams* params,
                 bool active_connector);

    /**

     */
    virtual ~CLConnection();

    /**
     * Attach to the given contact.
     */
    void set_contact(const ContactRef& contact) { contact_ = contact; }

protected:
    /**
     * Main run loop.
     */
    void run();

    /// @{
    /// Utility functions, all virtual so subclasses could override them
    virtual void contact_up();
    virtual void break_contact(ContactEvent::reason_t reason);
    virtual void process_command();
    virtual bool find_contact(const EndpointID& peer_eid);
    /// @}

    /**
     * Assignment function for the nexthop identifier
     */
    void set_nexthop(const std::string& nexthop) {
        nexthop_ = nexthop;
    }

    /**
     * Initiate a connection to the remote side.
     */
    virtual void connect() = 0;

    /**
     * Accept a connection from the remote side. For variants that
     * don't implement interfaces, but require a link to be configured
     * on both ends (e.g. serial), this will never be called, so the
     * base class simple asserts NOTREACHED.
     */
    virtual void accept() { NOTREACHED; }

    /**
     * Shut down the connection
     */
    virtual void disconnect() = 0;

    /**
     * Fill in the pollfds array with any file descriptors that should
     * be waited on for activity from the peer.
     */
    virtual void initialize_pollfds() = 0;

    /**
     * Handle notification that bundle(s) may be queued on the link.
     */
    virtual void handle_bundles_queued() = 0;

    /**
     * Handle a cancel bundle request
     */
    virtual void handle_cancel_bundle(Bundle* b) = 0;

    /**
     * Start or continue transmission of bundle data or cl acks. This
     * is called each time through the main run loop. Note that in
     * general, this function should send one "unit" of data, i.e. a
     * chunk of bundle data, a packet, etc.
     *
     * @returns true if some data was sent, which will trigger another
     * call, or false if the main loop should poll() on the socket
     * before calling again.
     */
    virtual bool send_pending_data() = 0;
    
    /**
     * Handle network activity from the remote side.
     */
    virtual void handle_poll_activity() = 0;

    /**
     * Handle network activity from the remote side.
     */
    virtual void handle_poll_timeout() = 0;

    /**
     * Enum for messages from the daemon thread to the connection
     * thread.
     */
    typedef enum {
        CLMSG_INVALID        = 0,
        CLMSG_BUNDLES_QUEUED = 1,
        CLMSG_CANCEL_BUNDLE  = 2,
        CLMSG_BREAK_CONTACT  = 3,
    } clmsg_t;

    /**
     * Message to string conversion.
     */
    const char* clmsg_to_str(clmsg_t type) {
        switch(type) {
        case CLMSG_INVALID:		return "CLMSG_INVALID";
        case CLMSG_BUNDLES_QUEUED:	return "CLMSG_BUNDLES_QUEUED";
        case CLMSG_CANCEL_BUNDLE:	return "CLMSG_CANCEL_BUNDLE";
        case CLMSG_BREAK_CONTACT:	return "CLMSG_BREAK_CONTACT";
        default:			PANIC("bogus clmsg_t");
        }
    }
    
    /**
     * struct used for messages going from the daemon thread to the
     * connection thread.
     */
    struct CLMsg {
        CLMsg()
            : type_(CLMSG_INVALID),
              bundle_("ConnectionConvergenceLayer::CLMsg") {}
        
        CLMsg(clmsg_t type)
            : type_(type),
              bundle_("ConnectionConvergenceLayer::CLMsg") {}
        
        CLMsg(clmsg_t type, const BundleRef& bundle)
            : type_(type),
              bundle_(bundle.object(), "ConnectionConvergenceLayer::CLMsg") {}
        
        clmsg_t   type_;
        BundleRef bundle_;
    };

    /**
     * Typedef for bitmaps used to record sent/received/acked data.
     */
    typedef oasys::SparseBitmap<u_int32_t> DataBitmap;
   
    /**
     * Struct used to record bundles that are in-flight along with
     * their transmission state and optionally acknowledgement data.
     */
    class InFlightBundle {
    public:
        InFlightBundle(Bundle* b)
            : bundle_(b, "CLConnection::InFlightBundle"),
              total_length_(0),
              send_complete_(false),
              transmit_event_posted_(false)
        {}
        
        BundleRef bundle_;
        BlockInfoVec* blocks_;

        u_int32_t total_length_;
        bool      send_complete_;
        bool      transmit_event_posted_;
        
        DataBitmap sent_data_;
        DataBitmap ack_data_;

    private:
        // make sure we don't copy the structure by leaving the copy
        // constructor undefined
        InFlightBundle(const InFlightBundle& copy);
    };

    /**
     * Typedef for the list of in-flight bundles.
     */
    typedef std::list<InFlightBundle*> InFlightList;

    /**
     * Struct used to record bundles that are in the process of being
     * received along with their transmission state and relevant
     * acknowledgement data.
     */
    class IncomingBundle {
    public:
        IncomingBundle(Bundle* b)
            : bundle_(b, "CLConnection::IncomingBundle"),
              bundle_complete_(false),
              bundle_accepted_(false),
              total_length_(0),
              acked_length_(0) {}

        BundleRef bundle_;
        
        bool bundle_complete_;
        bool bundle_accepted_;

        u_int32_t total_length_;
        u_int32_t acked_length_;

        DataBitmap rcvd_data_;
        DataBitmap ack_data_;
    private:
        // make sure we don't copy the structure by leaving the copy
        // constructor undefined
        IncomingBundle(const IncomingBundle& copy);
    };

    /**
     * Typedef for the list of in-flight bundles.
     */
    typedef std::list<IncomingBundle*> IncomingList;
    
    ContactRef          contact_;	///< Ref to the Contact
    bool		contact_up_;	///< Has contact_up been called
    oasys::SpinLock     cmdqueue_lock_; ///< Lock for command queue
    oasys::MsgQueue<CLMsg> cmdqueue_;	///< Daemon/CLConnection command queue
    ConnectionConvergenceLayer* cl_;	///< Pointer to the CL

    LinkParams*		params_;	///< Pointer to Link parameters, or
                                        ///< to defaults until Link is bound
    bool                active_connector_; ///< Should we connect() or accept()
    bool                active_connector_expects_keepalive_; ///< Should we connect() or accept()
    std::string		nexthop_;	///< Nexthop identifier set by CL
    int    		num_pollfds_;   ///< Number of pollfds in use
    static const int	MAXPOLL = 8;	///< Maximum number of pollfds
    struct pollfd       pollfds_[MAXPOLL]; ///< Array of pollfds
    int                 poll_timeout_;	///< Timeout to wait for poll data
    oasys::StreamBuffer sendbuf_; 	///< Buffer for outgoing data
    oasys::StreamBuffer recvbuf_;	///< Buffer for incoming data
    InFlightList	inflight_;	///< Bundles going out the wire
    IncomingList	incoming_;	///< Bundles arriving on the wire
    volatile bool	contact_broken_; ///< Contact has been broken
    oasys::atomic_t     num_pending_;	///< Bundles pending transmission
};

} // namespace dtn

#endif /* _CLCONNECTION_H_ */
