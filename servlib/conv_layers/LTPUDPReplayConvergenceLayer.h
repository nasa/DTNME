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
 *    results, designs or products rsulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _LTP_UDP_REPLAY_CONVERGENCE_LAYER_H_
#define _LTP_UDP_REPLAY_CONVERGENCE_LAYER_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef LTPUDP_ENABLED

#include <map>

#include "LTPUDPCommon.h"
#include "naming/EndpointID.h"
#include "IPConvergenceLayer.h"

#include <oasys/io/UDPClient.h>
#include <oasys/io/RateLimitedSocket.h>

using namespace std;

namespace dtn {

class LTPUDPSession;

class LTPUDPReplayConvergenceLayer : public IPConvergenceLayer,
                                     public LTPUDPCallbackIF {
public:
    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN = 65507;

    /**
     * Default port used by the udp cl.
     */
    static const u_int16_t LTPUDPREPLAYCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    LTPUDPReplayConvergenceLayer();
        
    /**
     * Destructor.
     */
    virtual ~LTPUDPReplayConvergenceLayer();
        
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);

    /**
     * Bring down the interface.
     */
    bool interface_down(Interface* iface);
    
    /**
     * Dump out CL specific interface information.
     */
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);

    /**
     * Create any CL-specific components of the Link.
     */
    bool init_link(const LinkRef& link, int argc, const char* argv[]);

    /**
     * Set default link options.
     */
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);

    /**
     * Delete any CL-specific components of the Link.
     */
    void delete_link(const LinkRef& link);

    /**
     * Dump out CL specific link information.
     */
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    
    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(const ContactRef& contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Send the bundle out the link.
     */
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

    u_int32_t Inactivity_Interval();

    void Cleanup_Replay_Session_Receiver(string session_key,int force); 
    virtual void Post_Timer_To_Process(oasys::Timer* event);

    LinkRef* link_ref() { return &link_ref_; }

    /**
     * Tunable parameter structure.
     *
     * Per-link and per-interface settings are configurable via
     * arguments to the 'link add' and 'interface add' commands.
     *
     * The parameters are stored in each Link's CLInfo slot, as well
     * as part of the Receiver helper class.
     */
    class Params : public CLInfo {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *a );

        in_addr_t  local_addr_;		 ///< Local address to bind to
        u_int16_t  local_port_;		 ///< Local port to bind to
        in_addr_t  remote_addr_;         ///< Peer address to connect to
        u_int16_t  remote_port_;	 ///< Peer port to connect to
        oasys::RateLimitedSocket::BUCKET_TYPE bucket_type_;         ///< bucket type for standard or leaky
        uint64_t   rate_;		 ///< Rate (in bps)
        u_int64_t  ttl_;		 ///< expiration of new bundle (seconds)
        u_int32_t  inactivity_intvl_;    ///< inactivity timer for segment to complete
        int        retran_intvl_;        ///< retransmit timer for report segment
        int        retran_retries_;      ///< retransmit report segment retries count
        uint64_t   bucket_depth_;	 ///< Token bucket depth (in bits)
        bool       wait_till_sent_;      ///< Force the socket to wait until sent on rate socket only
        EndpointIDPattern  dest_pattern_;        ///< place for real address.
        bool       no_filter_;           ///< filter is set or not
        int        inbound_cipher_suite_; ///< inbound cipher suite to use when authenticating.
        int        inbound_cipher_key_id_; ///< inbound cipher key id to use if set
        string     inbound_cipher_engine_;   ///< inbound engine EndpointID to use if set (0 | 1 suite)
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;

    Params session_params_;

  
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */

    class Receiver : public CLInfo,
                     public oasys::UDPClient,
                     public oasys::Thread
    {

    typedef std::map<std::string, LTPUDPSession *> SESS_MAP;

    public:

        /**
         * Constructor.
         */
        Receiver(LTPUDPReplayConvergenceLayer::Params* params, LTPUDPCallbackIF * parent);

        /**
         * Destructor.
         */
        virtual ~Receiver() {}

        /**
         * Loop forever, issuing blocking calls to IPSocket::recvfrom(),
         * then calling the process_data function when new data does
         * arrive
         *
         * Note that unlike in the Thread base class, this run() method is
         * public in case we don't want to actually create a new thread
         * for this guy, but instead just want to run the main loop.
         */
        void run();

        void Cleanup_Session(string session_key, int force) {
             // printf("Cleanup_Session----------------------------------------------------------------------------------------------\n");
             oasys::ScopeLock l(&rcvr_lock_,"RcvrCleanupSession");  // allow lock to happen
             cleanup_session(session_key, force);
        }

        LTPUDPReplayConvergenceLayer::Params params_;

        uint64_t udp_packets_received() { return udp_packets_received_; }

    protected:

        /**
         *  LTPDUPSession container
         */
        //u_int64_t dest_filter_node;
        //u_int64_t dest_filter_service;

        /// Lock to protect internal data structures and state.
        oasys::SpinLock rcvr_lock_;

        SESS_MAP ltpudpreplay_sessions_;
        SESS_MAP::iterator ltpudpreplay_session_iterator_;

        LTPUDPCallbackIF * parent_;

        uint64_t udp_packets_received_;
        uint64_t ltp_session_canceled_;


        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len); // processes LTP until full
        void process_all_data(u_char* bp, size_t len, bool mutlibundle); // processes Bundles stripped from LTP
        void track_session(u_int64_t engineid, u_int64_t sesionnumber);
        void cleanup_session(string session_key,int force);
    };

protected:

    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    virtual CLInfo* new_link_params();

    in_addr_t next_hop_addr_;
    u_int16_t next_hop_port_;
    int       next_hop_flags_; 
    Receiver *receiver_;

    LinkRef link_ref_;


    /*
     * Helper class that processes expiring timers
     */
    class TimerProcessor : public oasys::Thread,
                           public oasys::Logger {
    public:
        TimerProcessor();
        virtual ~TimerProcessor();
        virtual void run();
        virtual void post(oasys::Timer* event);
    protected:
        /// Message queue for accepting BundleEvents from ExternalRouter
        oasys::MsgQueue< oasys::Timer* > *eventq_;
    };

    TimerProcessor *timer_processor_;
};

} // namespace dtn

#endif // LTPUDP_ENABLED

#endif /* _UDP_CONVERGENCE_LAYER_H_ */
