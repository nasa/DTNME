#ifndef _CGBA_CONVERGENCE_LAYER_H_
#define _CGBA_CONVERGENCE_LAYER_H_

#include <oasys/io/UDPClient.h>
#include <oasys/thread/Thread.h>
#include <oasys/io/RateLimitedSocket.h>

#include "IPConvergenceLayer.h"
#include "bundling/Dictionary.h"

namespace dtn {

class CGBAConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Expected channel for a CGBA bundle set - channel 20
     */
    static const u_char CGBA_CHANNEL = 0x14;

    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN = 65507;

    /**
     * Maximum CGBA packet size
     */
    static const u_int MAX_CGBA_PACKET = 16636;

    /**
     * Maximum CGBA processing buffer size
     */
    static const u_int MAX_CGBA_BUFFER = MAX_CGBA_PACKET * 4;


        typedef enum ExtractState
        {
          ES_STATE_PRIME = 0,
          ES_STATE_PPRIME
        } ExtractState_t;


	typedef struct PrimaryBlock {
          u_int8_t version;
          u_int64_t processing_flags;
          u_int64_t block_length;
          u_int64_t dest_scheme_offset;
          u_int64_t dest_ssp_offset;
          u_int64_t source_scheme_offset;
          u_int64_t source_ssp_offset;
          u_int64_t replyto_scheme_offset;
          u_int64_t replyto_ssp_offset;
          u_int64_t custodian_scheme_offset;
          u_int64_t custodian_ssp_offset;
          u_int64_t creation_time;
          u_int64_t creation_sequence;
          u_int64_t lifetime;
          u_int64_t dictionary_length;
        } PrimaryBlock_t;

        typedef enum {
            CGBA_BUNDLE_IS_FRAGMENT             = 1 << 0,
            CGBA_BUNDLE_IS_ADMIN                = 1 << 1,
            CGBA_BUNDLE_DO_NOT_FRAGMENT         = 1 << 2,
            CGBA_BUNDLE_CUSTODY_XFER_REQUESTED  = 1 << 3,
            CGBA_BUNDLE_SINGLETON_DESTINATION   = 1 << 4,
            CGBA_BUNDLE_ACK_BY_APP              = 1 << 5,
            CGBA_BUNDLE_UNUSED                  = 1 << 6
        } bundle_processing_flag_t;

    /**
     * Default port used by the udp cl.
     */
    static const u_int16_t CGBACL_DEFAULT_PORT = 9923;
    
    /**
     * Constructor.
     */
    CGBAConvergenceLayer();
        
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
    void bundle_queued(const LinkRef& link, const BundleRef& bref);

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

        in_addr_t local_addr_;		///< Local address to bind to
        u_int16_t local_port_;		///< Local port to bind to
        in_addr_t remote_addr_;		///< Peer address to connect to
        u_int16_t remote_port_;		///< Peer port to connect to

        uint64_t rate_;		///< Rate (in bps)
        uint64_t bucket_depth_;	///< Token bucket depth (in bits)
    };
    

    /**
     * Default parameters.
     */
    static Params defaults_;

protected:
    /// @{ virtual from ConvergenceLayer
    virtual CLInfo* new_link_params();
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    /// @}


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
        Receiver(CGBAConvergenceLayer::Params* params);

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

        CGBAConvergenceLayer::Params params_;
        

        CGBAConvergenceLayer::ExtractState_t m_estate;

    protected:

        /**
         * A seperate udp client socket to handle receiving bundles
         * from EHS packets in a connectionless mode.
         */
        oasys::UDPClient no_conn_socket_;

        unsigned char* m_data_frame_ptr;
        // pointer to where current processing is
        int m_data_frame_len; 
        // bytes in EHS packet to process
        int m_remaining;  
        // m_remaining bytes accumulated in buffer to process
        int m_chan_pkt_len; 
        //length of chan pkt extracted from chan header
        int m_bytes_saved;
        // the number of bytes saved from a previous packet to 
        // be processed with the current packet
        int m_last_ric_count;
        int m_ric_count;
        unsigned char *m_save_buffer;
        unsigned char *m_save_packet_buffer;

        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, int *len, int *remaining);

        int uncompress_cbhe(u_char* bp, size_t len);

        void Extract_Process_Data(u_char* packet_buffer, size_t len);

        int Find_Channel_Hdr(
            unsigned char *pptr,            
            unsigned char **data_frame_ptr,
            int *chan_pkt_len,
            unsigned short *crc,
            int check_for_prime); 

        int Remove_Escaped_Bytes(
            unsigned char *pptr,
            unsigned int chan_pkt_len,
            unsigned int remaining,          
            unsigned int *escaped_bytes_used); 

        int Check_Escaped_Bytes(
            unsigned char *pptr,
            unsigned int chan_pkt_len,
            unsigned int remaining); 

        void hexdump(
            char title[],  void *start_addr,
            int length,  int no_char );

        int generate_primary(
            Dictionary *dict,
            u_int64_t frag_offset,
	    u_int64_t frag_length,
	    PrimaryBlock_t *primary,
	    u_char *buffer);

    };

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public CLInfo, public Logger {
    public:
        /**
         * Destructor.
         */
        virtual ~Sender() {}

        /**
         * Initialize the sender (the "real" constructor).
         */
        bool init(Params* params, in_addr_t addr, u_int16_t port);
        
    private:
        friend class CGBAConvergenceLayer;
        
        /**
         * Constructor.
         */
        Sender(const ContactRef& contact);
        
        /**
         * Send one bundle.
         * @return the length of the bundle sent or -1 on error
         */
        int send_bundle(const BundleRef& bref);

        /**
         * Pointer to the link parameters.
         */
        Params* params_;

        /**
         * The udp client socket.
         */
        oasys::UDPClient socket_;

        /**
         * Rate-limited socket that's optionally enabled.
         */
        oasys::RateLimitedSocket rate_socket_;
        
        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        /**
         * Temporary buffer for formatting bundles. Note that the
         * fixed-length buffer is big enough since UDP packets can't
         * be any bigger than that.
         */
        u_char buf_[CGBAConvergenceLayer::MAX_BUNDLE_LEN];
    };   
    Sender *sender;
};

} // namespace dtn

#endif /* _CGBA_CONVERGENCE_LAYER_H_ */
