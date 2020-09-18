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

#ifndef _LTP_UDP_CONVERGENCE_LAYER_H_
#define _LTP_UDP_CONVERGENCE_LAYER_H_

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif

#ifdef LTPUDP_ENABLED

#include <deque>
#include <map>
#include <stdarg.h>
#include <time.h>

#include <oasys/io/UDPClient.h>
#include <oasys/io/RateLimitedSocket.h>

#include "LTPUDPCommon.h"
#include "naming/EndpointID.h"
#include "IPConvergenceLayer.h"
#include "security/KeyDB.h"

using namespace std;

namespace dtn {


class LTPUDPSession;
class LTPUDPDataSegment;
class LTPUDPReportSegment;

typedef std::map<std::string, LTPUDPSession *>       SESS_MAP;
typedef std::map<std::string, LTPUDPDataSegment *>   DS_MAP;
typedef std::map<std::string, LTPUDPReportSegment *> RS_MAP;






class LTPSessionList
{
public:
    LTPSessionList() {} 
    virtual ~LTPSessionList() {}

    virtual bool add_session(LTPSessionKey& key, LTPUDPSession* session);

    virtual LTPUDPSession* remove_session(LTPSessionKey& key);

    virtual LTPUDPSession* find_session(LTPSessionKey& key);

    virtual size_t size();

private:
    oasys::SpinLock list_lock_;

    typedef std::map<LTPSessionKey, LTPUDPSession *>  LIST;
    typedef LIST::iterator                            LIST_ITER;
    typedef std::pair<LTPSessionKey, LTPUDPSession *>  LIST_PAIR;

    LIST list_;
};



class LTPUDPConvergenceLayer : public IPConvergenceLayer,
                               public LTPUDPCallbackIF,
                               public oasys::Thread {


public:
    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN    = 65507;
    static const u_int LTPUDP_MAX_LENGTH = 1430;

    /**
     * Default port used by the udp cl.
     */
    static const u_int16_t LTPUDPCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    LTPUDPConvergenceLayer();
    /**
     * Destructor
     */
    ~LTPUDPConvergenceLayer();

    /**
     * Perform any necessary shutdown procedures.
     */
    virtual void shutdown();
    virtual void wait_for_stopped(oasys::Thread* thread);

    /**
     * Thread run method
     */    
    void run();
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);

    /**
     * Start the interface running (after BundleDaemon signal)
     */
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
      * Reconfigure Link methods
     */
    bool reconfigure_link(const LinkRef& link,
                          int argc, const char* argv[]);
    void reconfigure_link(const LinkRef& link,
                          AttributeVector& params);
 
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
    bool pop_queued_bundle(const LinkRef& link);
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

    /**
     * CallbackIF prototypes
     */
    void Cleanup_Session_Sender(string session_key, string segment_key, int segment_type);   
    void Cleanup_Session_Receiver(string session_key, string segment_key, int segment_type);
    void Cleanup_RS_Receiver(string session_key, u_int64_t rs_key, int segment_type);
    //void Receiver_Generate_Checkpoint(string session_key, LTPUDPSegment * seg);
    void Process_Sender_Segment(LTPUDPSegment * segment);
    void Restart_Retransmit_Timer(int parent_type, int segment_type, std::string segment_key,
                                  std::string session_key, int report_serial_number, 
                                  u_int64_t target_counter);
    virtual void Post_Timer_To_Process(oasys::Timer* event);


    void Send(LTPUDPSegment * send_segment,int parent_type);
    void Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type);
    int  Retran_Interval();

    u_int32_t Inactivity_Interval();

    u_int64_t AOS_Counter() { return aos_counter_; }

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

        in_addr_t  local_addr_;		  ///< Local address to bind to
        u_int16_t  local_port_;		  ///< Local port to bind to
        in_addr_t  remote_addr_;          ///< Peer address to connect to
        u_int16_t  remote_port_;	  ///< Peer port to connect to
        oasys::RateLimitedSocket::BUCKET_TYPE bucket_type_;         ///< bucket type for standard or leaky
        uint64_t  rate_;		  ///< Rate (in bps)
        uint64_t  bucket_depth_;	  ///< Token bucket depth (in bits)
        int        inactivity_intvl_;     ///< inactivity timer for segment to complete
        int        retran_intvl_;         ///< retransmit timer for report segment
        int        retran_retries_;       ///< retransmit report segment retries count
        u_int64_t  engine_id_;		  ///< engine_id for sender to use 
        bool       wait_till_sent_;       ///< Force the socket to wait until sent on rate socket only
        bool       hexdump_;              ///< Dump the buffers in hex
        bool       comm_aos_;             ///< Are we AOS? 
        bool       ion_comp_;             ///< Use ION compatible LTP SDA instead of CCSDS spec (Service ID 2)
        u_int32_t  max_sessions_;         ///< Maximum Allowed session at any given time. 
        int        agg_size_;             ///< Aggregate size for a session (max 100MB network actual 100,000,000)
        int        agg_time_;             ///< milliseconds to wait until transmit if size not met.
        int        seg_size_;             ///< segment max size (64,000 max)
        u_int32_t  recvbuf_;              ///< set size for receive buffer
        u_int32_t  sendbuf_;              ///< set size for send buffer
        bool       clear_stats_;          ///< Transient signal to clear the statistics
        int        outbound_cipher_suite_;       ///< cipher suite type for which hash to use
        int        outbound_cipher_key_id_;      ///< cipher suite type for which hash to use
        string     outbound_cipher_engine_; ///< the cipher suite KeyDB engine to lookup
        int        inbound_cipher_suite_;        ///< cipher suite type for which hash to use
        int        inbound_cipher_key_id_;       ///< cipher suite type for which hash to use
        string     inbound_cipher_engine_;  ///< the cipher suite KeyDB engine to lookup
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

    public:

        /**
         * Constructor.
         */
        Receiver(LTPUDPConvergenceLayer::Params * params, LTPUDPCallbackIF * parent);

        /**
         * Destructor.
         */
        virtual ~Receiver();
        void do_shutdown();

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

        void Resend_RS(LTPUDPSession * session, LTPUDPReportSegment * seg);

        LTPUDPConvergenceLayer::Params cla_params_;
        LTPUDPReportSegment * scan_RS_list(LTPUDPSession * this_session);

        void Generate_Checkpoint(string session_key, LTPUDPSegment * seg);
        void Generate_Checkpoint(LTPUDPSession * session, LTPUDPSegment * seg);
        void Dump_Sessions(oasys::StringBuffer* buf);
        void Dump_Statistics(oasys::StringBuffer* buf);
        void Dump_Bundle_Processor(oasys::StringBuffer* buf);

        /**
         * Provide access to the lock and cleanup methods for RetransmitTimer processing
         */
        oasys::SpinLock* session_lock()   { return &session_lock_; }
        void cleanup_receiver_session(string session_key, string segment, int segment_type);
        void cleanup_RS(string session_key, string rs_key, int segment_type);

        u_int32_t  Outbound_Cipher_Suite();
        u_int32_t  Outbound_Cipher_Key_Id();
        string     Outbound_Cipher_Engine();

        virtual void clear_statistics();

        virtual void inc_bundles_received() { ++stats_.bundles_success_; }

        virtual int buffers_allocated() { return buffers_allocated_; }
        virtual size_t process_data_bytes_queued() { return data_process_offload_->bytes_queued(); }
        virtual size_t process_bundles_bytes_queued() { return bundle_processor_->bytes_queued(); }

        virtual void update_session_counts(LTPUDPSession* session, LTPUDPSession::SessionState_t);

        size_t sessions_state_ds;
        size_t sessions_state_rs;
        size_t sessions_state_cs;

    protected:

        // Poll the Link queue for bundles
        class RecvBundleProcessor: public Logger,
                                   public oasys::Thread        
        {
        public:
            RecvBundleProcessor(Receiver* parent, LinkRef* link);
            virtual ~RecvBundleProcessor();
            void run();
            virtual void post(u_char* data, size_t len, bool multi_bundle);
            virtual void extract_bundles_from_data(u_char* bp, size_t len, bool multi_bundle);
            virtual size_t queue_size();
            virtual size_t bytes_queued();
        protected:
            Receiver* parent_;
            LinkRef link_;

            oasys::SpinLock queue_lock_;
            size_t bytes_queued_;
            size_t max_bytes_queued_;

            struct EventObj {
                u_char* data_;
                size_t len_;
                bool multi_bundle_;
            };

            oasys::MsgQueue< struct EventObj* > *eventq_;

        };

        RecvBundleProcessor* bundle_processor_;


        struct DataProcObject {
            u_char* data_;
            size_t len_;
        };

        struct BufferObject {
            u_char* buffer_;
            size_t len_;
            size_t used_;
            DataProcObject* last_ptr_;
        };


        // DataProcessOffload
        class RecvDataProcessOffload: public Logger,
                                      public oasys::Thread        
        {
        public:
            RecvDataProcessOffload(Receiver* parent);
            virtual ~RecvDataProcessOffload();
            void run();
            virtual void post(DataProcObject*);
            virtual size_t queue_size();
            virtual size_t bytes_queued();
        protected:
            Receiver* parent_;

            oasys::SpinLock queue_lock_;
            size_t bytes_queued_;
            size_t max_bytes_queued_;

            oasys::MsgQueue<DataProcObject*> *eventq_;
        };

        RecvDataProcessOffload* data_process_offload_;

        virtual DataProcObject* get_dpo(int len);
        virtual void return_dpo(DataProcObject* obj);
        virtual char* get_recv_buffer();

        oasys::SpinLock dpo_lock_;
        std::deque<DataProcObject*> dpo_pool_;
        std::deque<BufferObject*> buffer_pool_;
        std::deque<BufferObject*> used_buffer_pool_;



        /**
         *  LTPDUPSession container
         */

        /// Lock to protect internal data structures and state.
        oasys::SpinLock session_lock_;

        RS_MAP::iterator   track_RS(LTPUDPSession * session, LTPUDPReportSegment * rpt_segment);
        SESS_MAP::iterator track_receiver_session(u_int64_t engineid, u_int64_t sesionnumber, bool create_flag);

        void build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code);

        SESS_MAP           ltpudp_rcvr_sessions_;
        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len); // processes LTP until full
        void cleanup_RS(string session_key, u_int64_t rs_key);
        void generate_RS(LTPUDPSession * report_session, int segment_type, size_t chkpt_upper_bounds);
	void generate_RS(LTPUDPSession * report_session, LTPUDPDataSegment* dataseg);
        int  build_RS_data(LTPUDPSession * session, u_char * buffer, int current_length, int seg_type);

        /// pointer back to parent
        LTPUDPCallbackIF * parent_;

        /// Statistics structure definition
        struct Stats {
            uint64_t ds_session_resends_;
            uint64_t ds_segment_resends_;
            uint64_t rs_session_resends_;
            uint64_t cs_session_resends_;
            uint64_t cs_session_cancel_by_sender_;
            uint64_t cs_session_cancel_by_receiver_;
            uint64_t cs_session_cancel_by_receiver_but_got_it_;
            uint64_t max_sessions_;

            uint64_t total_sessions_;
            uint64_t total_rcv_ds_;
            uint64_t total_rcv_ds_checkpoints_;
            uint64_t total_rcv_ra_;
            uint64_t total_rcv_ca_;
            uint64_t total_rcv_css_;
            uint64_t total_snt_rs_;
            uint64_t total_snt_csr_;
            uint64_t total_snt_ca_;

            uint64_t bundles_success_;
        };

        LTPUDPSession * blank_session_;
        /// Stats instance
        Stats stats_;
       
        int buffers_allocated_; 
    };

protected:

    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    virtual CLInfo* new_link_params();

    in_addr_t next_hop_addr_;
    u_int16_t next_hop_port_;
    int       next_hop_flags_; 

    u_int64_t aos_counter_;
    /**
     * create ltp header to be used to break up bundle.
     * builds using ltp_buf_  returns length
     */


    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public CLInfo, 
                   public Logger,
                   public oasys::Thread
    {

    public:
        /**
         * Constructor.
         */
        Sender(const ContactRef& contact, LTPUDPCallbackIF * parent);
        
        /**
         * Destructor.
         */
        virtual ~Sender();
        void do_shutdown();

        /**
         * Queue a bundle to be transmitted
         */
        int send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr_, u_int16_t next_hop_port_);

        /**
         * Initialize the sender (the "real" constructor).
         */
        bool init(Params* params, in_addr_t addr, u_int16_t port);
        bool reconfigure();
        void Send(LTPUDPSegment * send_segment,int parent_type);
        void Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type);
        void Resend_CancelSegment(LTPUDPSession * ltpsession);
        void Resend_Checkpoint(LTPUDPSession * ltpsession);
        void Sender_RS_Received(u_char * buf,int length);
        void Sender_CS_Received(u_char * buf,int length);
        void Sender_CAS_Receiver(u_char * buf,int length);
        void Sender_CAS_Sender(u_char * buf,int length);
        void Receiver_CAS_Received(u_char * buf,int length);
        void Send_Remaining_Segments(string session);
        void Send_Remaining_Segments(LTPUDPSession * session);
//dz        void generate_RS_ack(LTPUDPSession * report_session);
        void PostTransmitProcessing(LTPUDPSession * session, bool success=true);
        void Process_Segement(LTPUDPSegment * segment);
        void Resend_Checkpoint(string session_key);
        void Process_Segment(LTPUDPSegment * segment);
        void Dump_Sessions(oasys::StringBuffer* buf);
        void Dump_Statistics(oasys::StringBuffer* buf);
        void dump_link(oasys::StringBuffer* buf);

        void Post_Segment_to_Process(LTPUDPSegment* seg);
        LTPUDPSession * Blank_Session() { return blank_session_; }

        u_int32_t  Outbound_Cipher_Suite();
        u_int32_t  Outbound_Cipher_Key_Id();
        string     Outbound_Cipher_Engine();

        /**
         * Provide access to the lock and cleanup methods for RetransmitTimer processing
         */
        oasys::SpinLock* session_lock()   { return &session_lock_; }
        void cleanup_sender_session(string session_key, string segment, int segment_type);

        virtual void dump_queue_sizes(oasys::StringBuffer* buf);
        virtual void clear_statistics();

        virtual void update_session_counts(LTPUDPSession* session, LTPUDPSession::SessionState_t);

        size_t sessions_state_ds;
        size_t sessions_state_rs;
        size_t sessions_state_cs;

    protected:
        virtual bool check_ready_for_bundle();

        // Poll the Link queue for bundles
        class SendPoller: public Logger,
                          public oasys::Thread        
        {
        public:
            SendPoller(Sender* parent, const LinkRef& link);
            virtual ~SendPoller();
            void run();
        protected:
            Sender* parent_;
            LinkRef link_;
        };

        SendPoller* poller_;

        // ProcessOffload
        class SenderProcessOffload: public Logger,
                                    public oasys::Thread        
        {
        public:
            SenderProcessOffload(Sender* parent);
            virtual ~SenderProcessOffload();
            void run();
            virtual void post(LTPUDPSegment* seg);
            virtual bool generateACK(LTPUDPSegment* seg);
            virtual size_t queue_size();
        protected:
            Sender* parent_;

            oasys::SpinLock queue_lock_;
            oasys::MsgQueue<LTPUDPSegment*> *eventq_;
        };

        SenderProcessOffload* sender_process_offload_;

        // GenerateDataSegsOffload
        class SenderGenerateDataSegsOffload: public Logger,
                                             public oasys::Thread        
        {
        public:
            SenderGenerateDataSegsOffload(Sender* parent);
            virtual ~SenderGenerateDataSegsOffload();
            void init(Params* params);
            void run();
            virtual void post(LTPUDPSession* session);
            virtual size_t queue_size();
        protected:
            Sender* parent_;
            mutable Params *params_;

            oasys::SpinLock queue_lock_;
            oasys::MsgQueue<LTPUDPSession*> *eventq_;
        };

        SenderGenerateDataSegsOffload* sender_generate_datasegs_offload_;

    private:

        oasys::SpinLock session_lock_;
        oasys::SpinLock loading_session_lock_;

        LTPUDPSession * blank_session_;
        LTPUDPSession * loading_session_;

        Params *params_;

        LinkRef   * link_ref_;
        BundleRef * bundle_ref_;
  
        /// Message queue for accepting BundleEvents from Receiver
        oasys::MsgQueue< MySendObject * > *admin_sendq_;
        oasys::MsgQueue< MySendObject * > *high_eventq_;
        oasys::MsgQueue< MySendObject * > *low_eventq_;
 
        /**
         * Send one bundle.
         * @return the length of the bundle sent or -1 on error
         */
        size_t process_bundles(LTPUDPSession* loaded_session, bool is_green);
        /**
         * load up a session and save it's contents 
         */
        SESS_MAP::iterator  track_sender_session(u_int64_t engine_id, u_int64_t session_id, bool create_flag);

        void run();
        void build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code);
        void process_CS  (LTPUDPCancelSegment    * cs_segment);
        void process_CAS (LTPUDPCancelAckSegment * cas_segment);
        void process_RS  (LTPUDPReportSegment    * report_segment);

        /**
         * Pointer to the link parameters.
         */
        oasys::SpinLock sender_write_lock_;

        /**
         * The udp client socket.
         */
        oasys::UDPClient socket_;

        /**
         * session counter to keep everything unique
         */
        u_int64_t session_ctr_;

        /**
         * Rate-limited socket that's optionally enabled.
         */
        oasys::RateLimitedSocket* rate_socket_;
        
        /**
         * Session map for sender side and helper variables
         */
        SESS_MAP ltpudp_send_sessions_;

        LTPSessionList canceled_sessions_;

        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        /**
         * Temporary buffer for formatting bundles. Note that the
         * fixed-length buffer is big enough since UDP packets can't
         * be any bigger than that.
         */
        // u_char * buf_;
        /**
         * pointer back to parent
         */
        LTPUDPCallbackIF * parent_;


        /// Statistics structure definition
        struct Stats {
            uint64_t ds_session_resends_;
            uint64_t ds_session_checkpoint_resends_;
            uint64_t ds_segment_resends_;
            uint64_t cs_session_resends_;
            uint64_t cs_session_cancel_by_sender_;
            uint64_t cs_session_cancel_by_receiver_;
            uint64_t max_sessions_;

            uint64_t total_sessions_;
            uint64_t total_snt_ds_;
            uint64_t total_snt_ds_checkpoints_;
            uint64_t total_snt_ra_;
            uint64_t total_snt_ca_;
            uint64_t total_snt_css_;
            uint64_t total_rcv_rs_;
            uint64_t total_rcv_csr_;
            uint64_t total_rcv_ca_;

            uint64_t bundles_success_;
            uint64_t bundles_fail_;
        };
    
        /// Stats instance
        Stats stats_;

        size_t max_adminq_size_;
        size_t max_highq_size_;
        size_t max_lowq_size_;
            
    };   

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
        oasys::MsgQueue< oasys::Timer * > *eventq_;
    };

    LinkRef link_ref_;
    Sender   *sender_;
    Receiver *receiver_;
    TimerProcessor *timer_processor_;
};

} // namespace dtn

#endif // LTPUDP_ENABLED

#endif /* LTP_UDP_CONVERGENCE_LAYER_H_ */
