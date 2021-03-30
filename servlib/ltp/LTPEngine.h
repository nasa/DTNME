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

#ifndef _LTP__ENGINE_H_
#define _LTP__ENGINE_H_

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif


#include <deque>
#include <limits.h>
#include <map>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <third_party/oasys/thread/Thread.h>

#include "LTPCLSenderIF.h"
#include "LTPCommon.h"
#include "naming/EndpointID.h"

namespace dtn {

class LTPSession;
class LTPSegment;
class LTPDataSegment;
class LTPReportSegment;
class LTPNode;
class LTPEngine;
class LTPEngineReg;

typedef std::shared_ptr<LTPNode> SPtr_LTPNode;
typedef std::shared_ptr<LTPSession> SPtr_LTPSession;
typedef std::shared_ptr<LTPEngine> SPtr_LTPEngine;
typedef std::shared_ptr<LTPEngineReg> SPtr_LTPEngineReg;


class LTPEngineReg { 

public:

        LTPEngineReg(SPtr_LTPCLSenderIF& clsender);
        virtual ~LTPEngineReg();

        uint64_t           Remote_Engine_ID()  { return remote_engine_id_; }
        SPtr_LTPCLSenderIF CLSender()  { return clsender_;  }
        SPtr_LTPNode       LTP_Node()  { return ltpnode_;   }

        void Set_LTPNode(SPtr_LTPNode& node) { ltpnode_ = node; }

private:

        uint64_t           remote_engine_id_; // will hold engine id
        SPtr_LTPCLSenderIF clsender_;  // will hold IF to get back to clsender. 
                                       //     (has a shared_ptr to the LTPCL::Sender)
        SPtr_LTPNode       ltpnode_;   // will get created if not already running.
};

struct LTPDataReceivedEvent {
    u_char* data_   = nullptr;
    size_t len_     = 0;
    int  seg_type_  = -1;
    bool closed_    = false;
    bool cancelled_ = false;
};

class LTPEngine : public oasys::Logger
{

    /**
     * registration from CLSender
     */
public:


        typedef enum {
            LTP_SENDER_STATS_CMD,       // 0
            LTP_RECEIVER_STATS_CMD,     // 1
            LTP_SENDER_SESSIONS_CMD,    // 2
            LTP_RECEIVER_SESSIONS_CMD,  // 3
            LTP_BUNDLE_CMD,             // 4
            LTP_CLEAR_STATS_CMD,        // 5
            LTP_UNKNOWN_CMD             // 6
        } LTPCMD_t;


    /*
     * Constructor.
     */
    LTPEngine(uint64_t local_engine_id);

    /** 
     * Destructor (called at shutdown time).
     */
    virtual ~LTPEngine();


    /**
     * Queues the event at the tail of the queue for processing by the
     * daemon thread.
     */

    virtual bool register_engine(SPtr_LTPCLSenderIF& clsender);
    virtual void unregister_engine(uint64_t engine);

    virtual void post_data(u_char * buffer, size_t length);
    virtual int64_t send_bundle(const BundleRef& bundle, uint64_t engine);

    virtual SPtr_LTPEngineReg lookup_engine(uint64_t engine);
    virtual SPtr_LTPEngineReg lookup_engine(uint64_t engine, uint64_t session,bool& closed, bool& cancelled);
   
    virtual void link_reconfigured(uint64_t engine);
    virtual void dump_link_stats(uint64_t engine, oasys::StringBuffer* buf);
    virtual void clear_stats(uint64_t engine);
 
    virtual uint64_t local_engine_id() { return local_engine_id_; }
    virtual uint64_t new_session_id(uint64_t remote_engine_id);

    virtual void     closed_session(uint64_t session_id, uint64_t engine_id, bool cancelled);
    virtual void     delete_closed_session(uint64_t session_id);
    virtual size_t   get_closed_session_count();

    virtual void shutdown();

private:

    uint64_t local_engine_id_;
    uint64_t session_id_;

    oasys::SpinLock session_id_lock_; 
    oasys::SpinLock enginereg_map_lock_;

    typedef std::map<uint64_t, uint64_t>              SESS_CTR_TO_ENGINE_MAP;
    SESS_CTR_TO_ENGINE_MAP        session_id_to_engine_map_;

    struct ClosedSession {
        uint64_t engine_id_;
        bool     cancelled_;
    };

    typedef std::map<uint64_t, ClosedSession>         CLOSED_SESS_CTR_TO_ENGINE_MAP;
    CLOSED_SESS_CTR_TO_ENGINE_MAP closed_session_id_to_engine_map_;

    typedef std::map<uint64_t, SPtr_LTPEngineReg>     ENGINE_MAP;
    ENGINE_MAP registered_engines_;

    class RecvDataProcessor:public oasys::Logger,
                             public oasys::Thread
    {
        public:

            RecvDataProcessor();
            virtual ~RecvDataProcessor();
            virtual void shutdown();

            virtual void post_data(u_char* data, size_t len);
        protected:
            void run() override;

            SPtr_LTPEngine ltp_engine_sptr_;

            oasys::MsgQueue< struct LTPDataReceivedEvent* > eventq_;

            oasys::SpinLock eventq_lock_;
            size_t eventq_bytes_ = 0;
            size_t eventq_bytes_max_ = 0;
            size_t eventq_max_next_size_to_log_ = 10000000;
    };
    

    std::unique_ptr<RecvDataProcessor> data_processor_;


};

class LTPNode : public oasys::Logger,
                public oasys::Thread 
{

public:
    /**
     * Constructor.
     */
    LTPNode(SPtr_LTPEngine& ltp_engine_sptr, SPtr_LTPCLSenderIF&  cl_sender);

    /**
     * Destructor
     */
    ~LTPNode();

    /**
     * Post recevied data to be processed
     */
    virtual void post_data(LTPDataReceivedEvent* event);

    /**
     * AOS Counter Thread
     */    
    virtual void run() override;

    /**
     * shtudown processing
     */
    virtual void shutdown();

    virtual uint64_t AOS_Counter();

    /**
     * Dump out CL specific link information.
     */
    void reconfigured();
    virtual void dump_link_stats(oasys::StringBuffer* buf);
    virtual void clear_stats();

    /**
     * pass clsender method
     */ 
    SPtr_LTPCLSenderIF CLSender() { return clsender_; }
    uint64_t Remote_Engine_ID() { return clsender_->Remote_Engine_ID(); }


    int64_t send_bundle(const BundleRef& bundle);

    /**
     * LTPNodeIF prototypes
     */
    void Process_Sender_Segment(LTPSegment * segment, bool closed, bool cancelled);
    void Post_Timer_To_Process(oasys::SPtr_Timer& event);


    SPtr_LTPNodeRcvrSndrIF sender_rsif_sptr();
    SPtr_LTPNodeRcvrSndrIF receiver_rsif_sptr();

protected:
    // define a compare function for use in containers
    struct LTPSessionKeyCompare {
        bool operator()(LTPSessionKey* key1, LTPSessionKey* key2) {
            return ((key1->Engine_ID() < key2->Engine_ID()) || (key1->Session_ID() < key2->Session_ID()));
        }
    };

    typedef std::map<LTPSessionKey*, SPtr_LTPSession, LTPSessionKeyCompare>    SESS_SPTR_MAP;


    bool shutting_down_ = false;

    // parent LTPEngine object
    SPtr_LTPEngine ltp_engine_sptr_;


    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */

    class Receiver : public oasys::Logger,
                     public oasys::Thread,
                     public LTPNodeRcvrSndrIF
    {

    public:

        /**
         * Constructor.
         */
        Receiver(LTPNode* parent_node, SPtr_LTPCLSenderIF& clsender);

        /**
         * Destructor.
         */
        virtual ~Receiver();

        /**
         * Post recevied data to be processed
         */
        virtual void post_data(LTPDataReceivedEvent* event);

        /**
         * overrides for the LTPNodeRcvrSndrIF interface
         */
        void     RSIF_post_timer_to_process(oasys::SPtr_Timer& event);
        uint64_t RSIF_aos_counter();
        void     RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment& retran_seg);
        void     RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id);
        void     RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id);


        void reconfigured();
        void dump_sessions(oasys::StringBuffer* buf);
        void dump_success_stats(oasys::StringBuffer* buf);
        void dump_cancel_stats(oasys::StringBuffer* buf);

        /**
         * Provide access to the lock and cleanup methods for RetransmitTimer processing
         */
        void shutdown();

        oasys::SpinLock* xxsession_lock()   { return &session_list_lock_; }
        void cleanup_RS(uint64_t engine_id, uint64_t session_id, uint64_t report_serial_number, int event_type);

        uint32_t  Outbound_Cipher_Suite();
        uint32_t  Outbound_Cipher_Key_Id();
//        std::string& Outbound_Cipher_Engine();

        virtual void clear_statistics();

        uint64_t Remote_Engine_ID() { return clsender_->Remote_Engine_ID(); }

        virtual void process_data_for_receiver(LTPDataReceivedEvent* event);

        virtual void inc_bundles_success() { ++stats_.bundles_success_; }
        virtual void inc_bundles_failed() { ++stats_.bundles_failed_; }

        uint64_t ltp_queued_bytes_quota() { return clsender_->Ltp_Queued_Bytes_Quota(); }
    protected:
        virtual void run() override;

        virtual void process_data_for_sender(u_char * bp, size_t len, bool closed, bool cancelled);
        virtual void process_incoming_data_segment(SPtr_LTPSession& session_sptr, LTPSegment* seg);

        virtual void report_seg_timeout(SPtr_LTPSegment& seg);
        virtual void cancel_seg_timeout(SPtr_LTPSegment& seg);

        //
        // Poll the Link queue for bundles
        //
        class RecvSegProcessor : public oasys::Logger,
                                 public oasys::Thread
        {
        public:

            RecvSegProcessor(Receiver* parent_rcvr);
            virtual ~RecvSegProcessor();
            void run();
            void shutdown();
            virtual void post_admin(LTPDataReceivedEvent* event);
            virtual void post_ds(LTPDataReceivedEvent* event);
            virtual void get_queue_stats(size_t& queue_size, uint64_t& bytes_queued, 
                                         uint64_t& bytes_queued_max);

        protected:

            Receiver* parent_rcvr_;

            oasys::MsgQueue< LTPDataReceivedEvent* > eventq_admin_;
            oasys::MsgQueue< LTPDataReceivedEvent* > eventq_ds_;

            bool shutting_down_ = false;

            oasys::SpinLock eventq_ds_lock_;
            //uint64_t queued_bytes_quota_ = 2000000000; // default 2 GB
            size_t eventq_ds_bytes_ = 0;
            size_t eventq_ds_bytes_max_ = 0;
            size_t eventq_ds_max_next_size_to_log_ = 10000000;
        };

        typedef std::unique_ptr<RecvSegProcessor> QPtr_RecvSegProcessor;

        class RecvBundleProcessor:public oasys::Logger,
                                  public oasys::Thread
        {
        public:

            RecvBundleProcessor(Receiver* parent_rcvr);
            virtual ~RecvBundleProcessor();
            void run();
            void shutdown();
            virtual void post(u_char* data, size_t len, bool multi_bundle);
            virtual void extract_bundles_from_data(u_char* bp, size_t len, bool multi_bundle);
            virtual void get_queue_stats(size_t& queue_size, uint64_t& bytes_queued, 
                                         uint64_t& bytes_queued_max, uint64_t& bytes_quota);

            virtual void reconfigured();
            virtual bool okay_to_queue();

        protected:

            Receiver* parent_rcvr_;

            struct EventObj {
                u_char* data_;
                size_t len_;
                bool multi_bundle_;
            };

            oasys::MsgQueue< struct EventObj* > eventq_;

            oasys::SpinLock lock_;
            size_t queued_bytes_quota_ = 2000000000; // default 2 GB
            size_t eventq_bytes_ = 0;
            size_t eventq_bytes_max_ = 0;
            size_t eventq_max_next_size_to_log_ = 100000000;

            bool shutting_down_ = false;

        };
        typedef std::unique_ptr<RecvBundleProcessor> QPtr_RecvBundleProcessor;



        QPtr_RecvSegProcessor seg_processor_;
        QPtr_RecvBundleProcessor bundle_processor_;

//        struct DataProcObject {
//            u_char* data_;
//            size_t len_;
//        };

//        struct BufferObject {
//            u_char* buffer_;
//            size_t len_;
//            size_t used_;
//            DataProcObject* last_ptr_;
//        };

    protected:

        /// Lock to protect internal data structures and state.
        oasys::SpinLock session_list_lock_;

        virtual SPtr_LTPSession    find_incoming_session(LTPSegment* seg, bool create_flag);
        virtual SPtr_LTPSession    find_incoming_session(uint64_t engine_id, uint64_t session_id);
        virtual void               erase_incoming_session(SPtr_LTPSession& session_sptr);

        virtual void update_session_counts(SPtr_LTPSession& session, int new_state);

        virtual void build_CS_segment(SPtr_LTPSession& session, int segment_type, u_char reason_code);
        virtual void build_CAS_for_CSS(LTPCancelSegment* seg);

        SESS_SPTR_MAP           incoming_sessions_;

        /**
         * Handler to process an arrived packet.
         */
        virtual void generate_RS(SPtr_LTPSession& session_Sptr, LTPDataSegment * dataseg);


        SPtr_LTPCLSenderIF clsender_;

        /// pointer back to parent Node
        LTPNode* parent_node_;

        // local copies of the CLSender parameter values
        uint32_t seg_size_            = 1400;
        uint32_t retran_retries_      = 7;
        uint32_t retran_interval_     = 7;   // seconds
        uint32_t inactivity_interval_ = 30;   // seconds

        /// Statistics structure definition
        struct Stats {
            // success oriented stats
            uint64_t total_sessions_;
            uint64_t max_sessions_;

            uint64_t ds_sessions_with_resends_;
            uint64_t total_rcv_ds_;
            uint64_t ds_segment_resends_;

            uint64_t total_rs_segs_generated_;
            uint64_t rs_segment_resends_;
            uint64_t total_rcv_ra_;

            uint64_t bundles_success_;

            // cancel oriented stats
            uint64_t total_rcv_css_;
            uint64_t rcv_css_resends_;

            uint64_t total_snt_csr_;
            uint64_t csr_segment_resends_;
            uint64_t total_sent_and_rcvd_ca_;

            uint64_t session_cancelled_but_got_it_;
            uint64_t RAS_not_received_but_got_bundles_;

            uint64_t bundles_expired_in_queue_;
            uint64_t bundles_failed_;
        };

        bool shutting_down_ = false;

        oasys::MsgQueue< struct LTPDataReceivedEvent* > eventq_;

        oasys::SpinLock eventq_lock_;
        size_t eventq_bytes_ = 0;
        size_t eventq_bytes_max_ = 0;
        size_t eventq_max_next_size_to_log_ = 10000000;

        SPtr_LTPSession blank_session_;
        /// Stats instance
        Stats stats_;


        // Current Session Type Statistics
        size_t sessions_state_ds_ = 0;
        size_t sessions_state_rs_ = 0;
        size_t sessions_state_cs_ = 0;

        typedef std::map<uint64_t, uint64_t>  CLOSED_SESSION_MAP;
        CLOSED_SESSION_MAP closed_session_map_;
        size_t max_closed_sessions_ = 0;
    };

protected:

    uint64_t   aos_counter_;


    /// pointer back to ConvergenceLayer Sender
    SPtr_LTPCLSenderIF clsender_;

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public oasys::Logger,
                   public oasys::Thread,
                   public LTPNodeRcvrSndrIF
    {

    public:
        /**
         * Constructor.
         */
        Sender(SPtr_LTPEngine& ltp_engine_sptr, LTPNode* parent_node, SPtr_LTPCLSenderIF& clsender);
        
        /**
         * Destructor.
         */
        virtual ~Sender();

        /**
         * Queue a bundle to be transmitted
         */
        int64_t send_bundle(const BundleRef& bundle);

        /**
         * Perform shutdown processing
         */
        void shutdown();

        /**
         * overrides for the LTPNodeRcvrSndrIF interface
         */
        void     RSIF_post_timer_to_process(oasys::SPtr_Timer& event);
        uint64_t RSIF_aos_counter();
        void     RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment& retran_seg);
        void     RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id);
        void     RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id);


        void Resend_CancelSegment(SPtr_LTPSession& ltpsession);
        void Sender_RS_Received(u_char * buf,int length);
        void Sender_CS_Received(u_char * buf,int length);
        void Sender_CAS_Receiver(u_char * buf,int length);
        void Sender_CAS_Sender(u_char * buf,int length);
        void Receiver_CAS_Received(u_char * buf,int length);
        size_t Send_Remaining_Segments(SPtr_LTPSession& session);
        void PostTransmitProcessing(SPtr_LTPSession& session, bool success=true);
        void Process_Segment(LTPSegment * segment, bool closed, bool cancelled);

        void dump_sessions(oasys::StringBuffer* buf);
        void dump_success_stats(oasys::StringBuffer* buf);
        void dump_cancel_stats(oasys::StringBuffer* buf);

        uint32_t  Outbound_Cipher_Suite();
        uint32_t  Outbound_Cipher_Key_Id();
        std::string&     Outbound_Cipher_Engine();

        /**
         * Provide access to the lock and cleanup methods for RetransmitTimer processing
         */
        void cleanup_sender_session(uint64_t engine_id, uint64_t session_id, 
                                    uint64_t ds_start_byte, uint64_t ds_stop_byte, int event_type);

        virtual void clear_statistics();

        uint64_t Remote_Engine_ID() { return clsender_->Remote_Engine_ID(); }

        void reconfigured();
        uint64_t ltp_queued_bytes_quota() { return clsender_->Ltp_Queued_Bytes_Quota(); }

    protected:
        virtual void data_seg_timeout(SPtr_LTPSegment& seg);
        virtual void cancel_seg_timeout(SPtr_LTPSegment& seg);

        class SndrBundleProcessor:public oasys::Logger,
                                  public oasys::Thread
        {
        public:

            SndrBundleProcessor(Sender* parent_sndr);
            virtual ~SndrBundleProcessor();
            void run();
            void shutdown();
            virtual void post(SPtr_LTPSession& session_sptr, bool is_green);

            virtual void get_queue_stats(size_t& queue_size, uint64_t& bytes_queued, 
                                         uint64_t& bytes_queued_max);

            virtual bool okay_to_queue();

            virtual void reconfigured();

        protected:

            Sender* parent_sndr_;

            struct EventObj {
                SPtr_LTPSession session_sptr_;
                bool is_green_;
            };

            oasys::MsgQueue< struct EventObj* > eventq_;

            oasys::SpinLock lock_;
            uint64_t queued_bytes_quota_ = 2000000000; // default 2 GB
            uint64_t bytes_queued_ = 0;
            uint64_t bytes_queued_max_ = 0;

            bool shutting_down_ = false;

        };
        typedef std::unique_ptr<SndrBundleProcessor> QPtr_SndrBundleProcessor;



        QPtr_SndrBundleProcessor bundle_processor_;

    protected:

        /**
         * Send one bundle.
         * @return the length of the bundle sent or -1 on error
         */
        virtual void process_bundles(SPtr_LTPSession& loaded_session, bool is_green);
        /**
         * load up a session and save it's contents 
         */
        virtual bool create_sender_session();
        virtual SPtr_LTPSession find_sender_session(LTPSegment* seg);
        virtual SPtr_LTPSession find_sender_session(uint64_t engine_id, uint64_t session_id);
        virtual void erase_send_session(SPtr_LTPSession& session_sptr, bool cancelled);
        virtual void update_session_counts(SPtr_LTPSession& session, int new_state);

        virtual void run() override;
        virtual void build_CS_segment(SPtr_LTPSession& session, int segment_type, u_char reason_code);
        virtual void process_CS  (LTPCancelSegment    * cs_segment);
        virtual void process_CAS (LTPCancelAckSegment * cas_segment);
        virtual void process_RS  (LTPReportSegment    * report_segment, bool closed, bool cancelled);


    protected:
        oasys::SpinLock session_list_lock_;
        oasys::SpinLock loading_session_lock_;
        oasys::SpinLock session_id_map_lock_; 

        SPtr_LTPSession blank_session_;
        SPtr_LTPSession loading_session_;

        // local copies of the CLSender parameter values
        uint32_t max_sessions_        = 100;
        uint32_t agg_time_            = 500;      // millisecs
        uint64_t agg_size_            = 1000000;
        uint32_t seg_size_            = 1400;
        bool     ccsds_compatible_    = false;
        uint32_t retran_retries_      = 7;
        uint32_t retran_interval_     = 7;   // seconds
        uint32_t inactivity_interval_ = 30;   // seconds


        /**
         * Session map for sender side and helper variables
         */
        SESS_SPTR_MAP send_sessions_;

        /// Statistics structure definition
        struct Stats {
            // success oriented stats
            uint64_t total_sessions_;
            uint64_t max_sessions_;

            uint64_t ds_sessions_with_resends_;
            uint64_t total_snt_ds_;
            uint64_t ds_segment_resends_;

            uint64_t total_rcv_rs_;
            uint64_t rs_segment_resends_;
            uint64_t total_snt_ra_;

            uint64_t bundles_success_;


            // cancel oriented stats
            uint64_t total_snt_css_;
            uint64_t css_segment_resends_;

            uint64_t total_rcv_csr_;
            uint64_t csr_segment_resends_;
            uint64_t total_sent_and_rcvd_ca_;

            uint64_t bundles_expired_in_queue_;
            uint64_t bundles_failed_;
        };
    
        /// Stats instance
        Stats stats_;

        /// pointer back to ConvergenceLayer Sender
        SPtr_LTPCLSenderIF clsender_;
        LTPNode* parent_node_;

        // LTPEngine object
        SPtr_LTPEngine ltp_engine_sptr_;
        uint64_t local_engine_id_ = 0;

        uint64_t remote_engine_id_ = 0;

        bool shutting_down_ = false;

        // Current Session Type Statistics
        size_t sessions_state_ds_ = 0;
        size_t sessions_state_rs_ = 0;
        size_t sessions_state_cs_ = 0;
    };   

    /*
     * Helper class that processes expiring timers
     */
    class TimerProcessor : public oasys::Logger,
                           public oasys::Thread
    {
                           
    public:
        TimerProcessor(uint64_t remote_engine_id);
        virtual ~TimerProcessor();
        virtual void run();
        virtual void post(oasys::SPtr_Timer& event);
    protected:
        /// Message queue for accepting BundleEvents from ExternalRouter
        oasys::MsgQueue<oasys::SPtr_Timer> eventq_;
        uint64_t remote_engine_id_;
    };

    typedef std::shared_ptr<Sender> SPtr_Sender;
    typedef std::shared_ptr<Receiver> SPtr_Receiver;

    SPtr_Sender   sender_sptr_;
    SPtr_Receiver receiver_sptr_;
    std::unique_ptr<TimerProcessor> timer_processor_;

};

} // namespace dtn

#endif /* LTP_ENGINE_LAYER_H_ */
