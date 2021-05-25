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


#include <third_party/oasys/io/UDPClient.h>
#include <third_party/oasys/io/RateLimitedSocket.h>

#include "ltp/LTPEngine.h"
#include "IPConvergenceLayer.h"


namespace dtn {


class LTPUDPConvergenceLayer : public IPConvergenceLayer {

public:
    /**
     * Default port used by the udp cl.
     */
    static const uint16_t LTPUDPCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    LTPUDPConvergenceLayer();
    /**
     * Destructor
     */
    ~LTPUDPConvergenceLayer();

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
     * List valid options for links and interfaces
     */
    void list_link_opts(oasys::StringBuffer& buf);
    void list_interface_opts(oasys::StringBuffer& buf);

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

        in_addr_t   local_addr_               = INADDR_ANY;             ///< Local address to bind to
        uint16_t    local_port_               = LTPUDPCL_DEFAULT_PORT;  ///< Local port to bind to
        in_addr_t   remote_addr_              = INADDR_NONE;            ///< Peer address to connect to
        uint16_t    remote_port_              = LTPUDPCL_DEFAULT_PORT;  ///< Peer port to connect to

        uint64_t    rate_                     = 0;              ///< Rate (in bps)
        uint64_t    bucket_depth_             = 65535 * 8;      ///< Token bucket depth (in bits)
        uint32_t    inactivity_intvl_         = 30;             ///< inactivity timer for segment to complete
        uint32_t    retran_intvl_             = 7;              ///< retransmit timer for report segment
        uint32_t    retran_retries_           = 7;              ///< retransmit report segment retries count
        uint64_t    remote_engine_id_         = 0;              ///< engine_id of the Peer
        bool        hexdump_                  = false;          ///< Dump the buffers in hex
        bool        comm_aos_                 = true;           ///< Are we AOS? 
        uint32_t    max_sessions_             = 100;            ///< Maximum Allowed session at any given time. 
        uint64_t    agg_size_                 = 1000000;        ///< Aggregate size for a session (max 100MB network actual 100,000,000)
        uint32_t    agg_time_                 = 500;            ///< milliseconds to wait until transmit if size not met.
        uint32_t    seg_size_                 = 1400;           ///< segment max size (64,000 max)
        bool        ccsds_compatible_         = false;          ///< CCSDS compatibility (uses Client Service ID = 2 to signal Data Aggregation)
        uint32_t    recvbuf_                  = 0;              ///< set size for receive buffer
        uint32_t    sendbuf_                  = 0;              ///< set size for send buffer
        bool        clear_stats_              = false;          ///< Transient signal to clear the statistics
        bool        dump_sessions_            = true;           ///< Whether link dump report should detail the sessions
        bool        dump_segs_                = false;          ///< Whether link dump report should detail the red segments of sessions

        int         outbound_cipher_suite_    = -1;             ///< cipher suite type for which hash to use
        int         outbound_cipher_key_id_   = -1;             ///< cipher suite type for which hash to use
        std::string outbound_cipher_engine_;                    ///< the cipher suite KeyDB engine to lookup

        int         inbound_cipher_suite_     = -1;             ///< cipher suite type for which hash to use
        int         inbound_cipher_key_id_    = -1;             ///< cipher suite type for which hash to use
        std::string inbound_cipher_engine_;                     ///< the cipher suite KeyDB engine to lookup

        size_t      ltp_queued_bytes_quota_   = 2000000000;     ///< max LTP bytes to queue for bundle extraction before discarding DataSegments
                                                                ///<  due to insufficient resources (default: 2GB; 0 = no limit)
        size_t      bytes_per_checkpoint_     = 0;              ///< Send intermediate checkpoint every x bytes (if greater than zero; default: 0))
                                                                ///< NOTE: ION does not support intermediate checkpointsas 05/14/2021

        bool        use_files_xmit_          = false;           ///< Whether to use files to store outgoing LTP session data
        bool        use_files_recv_          = false;           ///< Whether to use files to store incoming LTP session data
        bool        keep_aborted_files_      = false;           ///< Whether to keep sessions files that had BP protocol errors
        bool        use_diskio_kludge_       = false;           ///< Whether to use Disk I/O kludge that was needed on the development system
        std::string dir_path_;                                  ///< Directory to use for storing LTP data files (default: local dir)

        int32_t     xmit_test_                = 0;              ///< specified which transmit test mode to run:
                                                                ///< 0 or 1 = normal mode, >1 = drop every nth packet to transmit
        int32_t     recv_test_                = 0;              ///< specified which receiver test mode to run:
                                                                ///< 0 or 1 = normal mode, >1 = drop every nth packet to transmit


        uint32_t   recvbuf_actual_size_      = 0;               ///< Set after opening socket for display purposes

        ///< bucket type for standard or leaky
        oasys::RateLimitedSocket::BUCKET_TYPE bucket_type_ = oasys::RateLimitedSocket::STANDARD;
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;

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
        Receiver(LTPUDPConvergenceLayer::Params * params, LTPUDPConvergenceLayer * parent);

        /**
         * Destructor.
         */
        virtual ~Receiver();

        /**
         * Loop forever, issuing blocking calls to IPSocket::recvfrom(),
         * then calling the process_data function when new data does
         * arrive
         *
         * Note that unlike in the Thread base class, this run() method is
         * public in case we don't want to actually create a new thread
         * for this guy, but instead just want to run the main loop.
         */
        void run() override;

        LTPUDPConvergenceLayer::Params cla_params_;

    protected:

        /**
         *  LTPDUPSession container
         */

        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len); // processes LTP until full

        /// pointer back to parent
        LTPUDPConvergenceLayer * parent_;
    };

protected:

    bool parse_params(Params* params, int argc, const char* argv[],
                      const char** invalidp);
    virtual CLInfo* new_link_params();

    u_int64_t  aos_counter_; 

    in_addr_t next_hop_addr_;
    u_int16_t next_hop_port_;
    int       next_hop_flags_; 

    /**
     * create ltp header to be used to break up bundle.
     * builds using ltp_buf_  returns length
     */


    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class LTPUDPSender : public CLInfo, 
                   public LTPCLSenderIF,
                   public Logger,
                   public oasys::Thread
    {

    public:
        /**
         * Constructor.
         */
        LTPUDPSender(const ContactRef& contact, LTPUDPConvergenceLayer * parent);
        
        /**
         * Stores a shared pointer to itself to provide it to the LTPEngine
         * @param sptr the shared pointer to this instance of the LTPUDPSender
         */
        void set_sptr(SPtr_LTPCLSenderIF sptr) { sptr_self_ = sptr; }
        
        /**
         * Destructor.
         */
        virtual ~LTPUDPSender();
        virtual void shutdown();

        /**
         * Initialize the sender (the "real" constructor).
         */
        bool init(Params* params, in_addr_t addr, u_int16_t port);
        bool reconfigure(Params* new_params);
        void dump_link(oasys::StringBuffer* buf);
        void dump_stats(oasys::StringBuffer* buf);

        /**
         * virtuals from ltp/LTPCLSenderIF.h
         */
        virtual u_int64_t      Remote_Engine_ID() override;

        virtual void           Set_Ready_For_Bundles(bool input_flag) override { ready_for_bundles_ = input_flag; }
        virtual void           Send_Admin_Seg_Highest_Priority(std::string * send_data, SPtr_LTPTimer timer, bool back) override;
        virtual void           Send_DataSeg_Higher_Priority(SPtr_LTPDataSegment ds_seg, SPtr_LTPTimer timer) override;
        virtual void           Send_DataSeg_Low_Priority(SPtr_LTPDataSegment ds_seg, SPtr_LTPTimer timer) override;
        virtual uint32_t       Retran_Intvl() override;
        virtual uint32_t       Retran_Retries() override;
        virtual uint32_t       Inactivity_Intvl() override;
        virtual void           Add_To_Inflight(const BundleRef& bundle) override;
        virtual void           Del_From_Queue(const BundleRef&  bundle) override;
        virtual bool           Del_From_InFlight_Queue(const BundleRef&  bundle) override;
        virtual size_t         Get_Bundles_Queued_Count() override;
        virtual size_t         Get_Bundles_InFlight_Count() override;
        virtual void           Delete_Xmit_Blocks(const BundleRef&  bundle) override;
        virtual uint32_t       Max_Sessions() override;
        virtual uint32_t       Agg_Time() override;
        virtual uint64_t       Agg_Size() override;
        virtual uint32_t       Seg_Size() override;
        virtual bool           CCSDS_Compatible() override;
        virtual size_t         Ltp_Queued_Bytes_Quota() override;
        virtual size_t         Bytes_Per_CheckPoint() override;
        virtual bool           Use_Files_Xmit() override;
        virtual bool           Use_Files_Recv() override;
        virtual bool           Keep_Aborted_Files() override;
        virtual bool           Use_DiskIO_Kludge() override;
        virtual int32_t        Recv_Test() override;
        virtual std::string&   Dir_Path() override;
        virtual void           PostTransmitProcessing(BundleRef& bref, bool red, uint64_t  expected_bytes, bool success) override;
        virtual SPtr_BlockInfoVec GetBlockInfoVec(Bundle * bundle) override;
        virtual bool           Dump_Sessions() override;
        virtual bool           Dump_Segs() override;
        virtual bool           Hex_Dump() override;
        virtual bool           AOS() override;
        virtual uint32_t       Inbound_Cipher_Suite()  override;
        virtual uint32_t       Inbound_Cipher_Key_Id() override;
        virtual std::string&   Inbound_Cipher_Engine() override;
        virtual uint32_t       Outbound_Cipher_Suite( ) override;
        virtual uint32_t       Outbound_Cipher_Key_Id() override;
        virtual std::string&   Outbound_Cipher_Engine() override;

    protected:
        typedef struct MySendObject {
            std::string*      str_data_ = nullptr;
            size_t            bytes_queued_ = 0;
            SPtr_LTPTimer     timer_;
            SPtr_LTPDataSegment ds_seg_;
        } MySendObject;


        virtual bool check_ready_for_bundle();
 
        // Poll the Link queue for bundles
        class SendPoller: public Logger,
                          public oasys::Thread
        {
        public:
            SendPoller(LTPUDPSender* parent, const LinkRef& link);
            virtual ~SendPoller();
            virtual void run() override;
            virtual void shutdown();
        protected:
            LTPUDPSender* parent_;
            LinkRef link_ref_;
        };

        std::unique_ptr<SendPoller> poller_;

    private:

        bool      ready_for_bundles_ = false;

        /// Message queue for accepting data to transmit
        oasys::MsgQueue< MySendObject * > admin_eventq_;
        oasys::MsgQueue< MySendObject * > ds_high_eventq_;
        oasys::MsgQueue< MySendObject * > ds_low_eventq_;
 
        size_t packets_dropped_for_xmit_test_ = 0;
        size_t packets_transmitted_ = 0;

        LinkRef     link_ref_;
        Params*     params_ = nullptr;
  
        /**
         * load up a session and save it's contents 
         */
        void run() override;

        /**
         * The udp client socket.
         */
        oasys::UDPClient socket_;

        /**
         * Rate-limited socket that's optionally enabled.
         */
        bool use_rate_socket_ = false;
        std::unique_ptr<oasys::RateLimitedSocket> rate_socket_;
        
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
        LTPUDPConvergenceLayer * parent_;

        SPtr_LTPCLSenderIF sptr_self_;

        oasys::SpinLock eventq_lock_;
        size_t eventq_bytes_ = 0;
        size_t eventq_bytes_max_ = 0;
    };   


    typedef std::shared_ptr<LTPUDPSender> SPtr_LTPUDPSender;
};


} // namespace dtn


#endif /* LTP_UDP_CONVERGENCE_LAYER_H_ */
