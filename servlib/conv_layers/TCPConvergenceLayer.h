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

#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include <third_party/oasys/io/TCPClient.h>
#include <third_party/oasys/io/TCPServer.h>
#include <third_party/oasys/serialize/Serialize.h>

#include "StreamConvergenceLayer.h"


#ifdef WOLFSSL_TLS_ENABLED
    #ifndef WOLFSSL_TLS13
    #    define WOLFSSL_TLS13 1
    #endif

    class WOLFSSL_CTX;
    class WOLFSSL;
#endif // WOLFSSL_TLS_ENABLED

namespace dtn {

/**
 * The TCP Convergence Layer.
 */
class TCPConvergenceLayer : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const uint8_t TCPCL_VERSION = 0x04;

    /**
     * Default port used by the tcp cl.
     */
    static const uint16_t TCPCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    TCPConvergenceLayer();

    /**
     * Destructor.
     */
    virtual ~TCPConvergenceLayer() {}

    /// @{ Virtual from ConvergenceLayer
    virtual bool interface_up(Interface* iface, int argc, const char* argv[]) override;
    virtual void interface_activate(Interface* iface) override;
    virtual bool interface_down(Interface* iface) override;
    virtual void dump_interface(Interface* iface, oasys::StringBuffer* buf) override;
    virtual void list_interface_opts(oasys::StringBuffer& buf) override;
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class TCPLinkParams : public StreamLinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *);

        bool      hexdump_ = false;                     ///< Log a hexdump of all traffic
        in_addr_t local_addr_ = INADDR_ANY;             ///< Local address to bind to
        uint16_t  local_port_ = TCPCL_DEFAULT_PORT;     ///< Local port to bind to
        in_addr_t remote_addr_ = INADDR_NONE;           ///< Peer address used for rcvr-connect
        uint16_t  remote_port_ = TCPCL_DEFAULT_PORT;    ///< Peer port used for rcvr-connect

        uint32_t  delay_for_tcpcl3_ = 5;                ///< seconds to delay waiting for TCPCL3 contact header
        uint32_t  peer_tcpcl_version_ = 0;              ///< Peer's TCP CL version set on connection


        uint64_t  max_rcv_seg_len_ = 10000000; ///< Maximum size of received segments (10 MB)
        uint64_t  max_rcv_bundle_size_ = 2000000000; ///< Maximum size for an incoming bundle (2 GB)

        bool tls_enabled_ = false;         ///< whether to advertise TLS capability
        bool require_tls_ = false;        ///< wheter to force use of TLS
        bool tls_active_ = false;         ///< negotiated state for reporting purposes (not configurable)

        std::string tls_iface_cert_file_ = "./certs/server/server-cert.pem";
        std::string tls_iface_cert_chain_file_ = "";
        std::string tls_iface_private_key_file_ = "./certs/server/server-key.pem";
        std::string tls_iface_verify_cert_file_ = "";
        std::string tls_iface_verify_certs_dir_ = "./certs/server/verify";

        std::string tls_link_cert_file_ = "./certs/client/client-cert.pem";
        std::string tls_link_cert_chain_file_ = "";
        std::string tls_link_private_key_file_ = "./certs/client/client-key.pem";
        std::string tls_link_verify_cert_file_ = "";
        std::string tls_link_verify_certs_dir_ = "./certs/client/verify";

        virtual ~TCPLinkParams();

        /**
         * Operator copy assignment
         */
        TCPLinkParams& operator=(const TCPLinkParams& other)
        {
            if (this != &other) {
                hexdump_ = other.hexdump_;
                local_addr_ = other.local_addr_;
                local_port_ = other.local_port_;
                remote_addr_ = other.remote_addr_;
                remote_port_ = other.remote_port_;

                delay_for_tcpcl3_ = other.delay_for_tcpcl3_;
                peer_tcpcl_version_ = other.peer_tcpcl_version_;

                max_rcv_seg_len_ = other.max_rcv_seg_len_;
                max_rcv_bundle_size_ = other.max_rcv_bundle_size_;

                tls_enabled_ = other.tls_enabled_;
                require_tls_ = other.require_tls_;
                tls_active_ = other.tls_active_;

                tls_iface_cert_file_ = other.tls_iface_cert_file_;
                tls_iface_cert_chain_file_ = other.tls_iface_cert_chain_file_;
                tls_iface_private_key_file_ = other.tls_iface_private_key_file_;
                tls_iface_verify_cert_file_ = other.tls_iface_verify_cert_file_;
                tls_iface_verify_certs_dir_ = other.tls_iface_verify_certs_dir_;

                tls_link_cert_file_ = other.tls_link_cert_file_;
                tls_link_cert_chain_file_ = other.tls_link_cert_chain_file_;
                tls_link_private_key_file_ = other.tls_link_private_key_file_;
                tls_link_verify_cert_file_ = other.tls_link_verify_cert_file_;
                tls_link_verify_certs_dir_ = other.tls_link_verify_certs_dir_;
            }

            return *this;
        }

    protected:
        friend class TCPConvergenceLayer;

        // See comment in LinkParams for why this is protected
        TCPLinkParams(bool init_defaults);
        TCPLinkParams(const TCPLinkParams& other);

    };

    /**
     * Default link parameters.
     */
    TCPLinkParams default_link_params_;

protected:
    /**
     * Values for ContactHeader flags.
     */
    typedef enum {
        TLS_CAPABLE   = 1 << 0,    ///< TLS can be used
    } tcpv4_contact_header_flags_t;

    typedef enum : uint8_t {
        TCPV4_MSG_TYPE_XFER_SEGMENT = 1,
        TCPV4_MSG_TYPE_XFER_ACK,
        TCPV4_MSG_TYPE_XFER_REFUSE,
        TCPV4_MSG_TYPE_KEEPALIVE,
        TCPV4_MSG_TYPE_SESS_TERM,
        TCPV4_MSG_TYPE_MSG_REJECT,
        TCPV4_MSG_TYPE_SESS_INIT
    } tcpv4_message_types_t;

    typedef enum : uint8_t {
        TCPV4_REFUSE_REASON_UNKNOWN = 0,
        TCPV4_REFUSE_REASON_COMPLETED,
        TCPV4_REFUSE_REASON_NO_RESOURCES,
        TCPV4_REFUSE_REASON_RETRANSMIT,
        TCPV4_REFUSE_REASON_NOT_ACCEPTABLE,
        TCPV4_REFUSE_REASON_EXTENSION_FAILURE
    } tcpv4_refusal_reasons_t;

    /**
     * Convert a reason code to a string.
     */
    static const char* tcpv4_refusal_reason_to_str(uint8_t reason)
    {
        switch (reason) {
        case TCPV4_REFUSE_REASON_COMPLETED:         return "completed";
        case TCPV4_REFUSE_REASON_NO_RESOURCES:      return "no resources";
        case TCPV4_REFUSE_REASON_RETRANSMIT:        return "retransmit";
        case TCPV4_REFUSE_REASON_NOT_ACCEPTABLE:    return "not acceptable";
        case TCPV4_REFUSE_REASON_EXTENSION_FAILURE: return "extension failure";
        default: return "unknown";
        }
        NOTREACHED;
    }
    
    typedef enum : uint8_t {
        TCPV4_SHUTDOWN_FLAG_REPLY = 1
    } tcpv4_shutdown_flags_t;

    typedef enum : uint8_t {
        TCPV4_SHUTDOWN_REASON_UNKNOWN = 0,
        TCPV4_SHUTDOWN_REASON_IDLE_TIMEOUT,
        TCPV4_SHUTDOWN_REASON_VERSION_MISMATCH,
        TCPV4_SHUTDOWN_REASON_BUSY,
        TCPV4_SHUTDOWN_REASON_CONTACT_FAILURE,
        TCPV4_SHUTDOWN_REASON_RESOURCE_EXHAUSTION
    } tcpv4_shutdown_reasons_t;

    /**
     * Convert a reason code to a string.
     */
    static const char* tcpv4_shutdown_reason_to_str(uint8_t reason)
    {
        switch (reason) {
        case TCPV4_SHUTDOWN_REASON_IDLE_TIMEOUT:        return "idle timeout";
        case TCPV4_SHUTDOWN_REASON_VERSION_MISMATCH:    return "version mismatch";
        case TCPV4_SHUTDOWN_REASON_BUSY:                return "busy";
        case TCPV4_SHUTDOWN_REASON_CONTACT_FAILURE:     return "contact failure";
        case TCPV4_SHUTDOWN_REASON_RESOURCE_EXHAUSTION: return "resource exhustion";
        default: return "unknown";
        }
        NOTREACHED;
    }
    

    typedef enum : uint8_t {
        TCPV4_MSG_REJECT_REASON_UNKNOWN = 1,
        TCPV4_MSG_REJECT_REASON_UNSUPPORTED,
        TCPV4_MSG_REJECT_REASON_UNEXPECTED
    } tcpv4_msg_reject_reasons_t;

    static const char* msg_reject_reason_to_str(uint8_t reason)
    {
        switch (reason) {
        case TCPV4_MSG_REJECT_REASON_UNKNOWN: return "unknown type";
        case TCPV4_MSG_REJECT_REASON_UNSUPPORTED: return "unsupported";
        case TCPV4_MSG_REJECT_REASON_UNEXPECTED:  return "unexpected";
        default: return "unknown reason";
        }
        NOTREACHED;
    }


    typedef enum : uint8_t {
        TCPV4_EXTENSION_FLAG_CRITICAL = 1
    } tcpv4_extension_flags_t;

    typedef enum : uint8_t {
        TCPV4_EXTENSION_TYPE_TRANSFER_LENGTH = 1
    } tcpv4_extension_itmes_types_t;

    /**
     * Contact initiation header. Sent at the beginning of a contact. 
     */
    struct TCPv4ContactHeader {
        u_int32_t magic;        ///< magic word (MAGIC: "dtn!")
        u_int8_t  version;        ///< cl protocol version
        u_int8_t  flags;        ///< connection flags (see above)
    } __attribute__((packed));


    /// @{ Virtual from ConvergenceLayer
    virtual bool init_link(const LinkRef& link, int argc, const char* argv[]) override;
    virtual bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp) override;
    virtual void dump_link(const LinkRef& link, oasys::StringBuffer* buf) override;
    virtual void list_link_opts(oasys::StringBuffer& buf) override;
    /// @}
    
    /// @{ Virtual from ConvergenceLayer
    virtual CLInfo* new_link_params() override;
    virtual bool parse_link_params(LinkParams* params,
                                   int argc, const char** argv,
                                   const char** invalidp) override;
    virtual bool parse_nexthop(const LinkRef& link, LinkParams* params) override;
    virtual CLConnection* new_connection(const LinkRef& link,
                                         LinkParams* params) override;
    /// @}

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::TCPServerThread {
    public:
        Listener(TCPConvergenceLayer* cl, TCPLinkParams* params);
        virtual ~Listener();

        virtual void dump_interface(oasys::StringBuffer* buf);
        void virtual accepted(int fd, in_addr_t addr, u_int16_t port);

        /// The TCPCL instance
        TCPConvergenceLayer* cl_;

        TCPLinkParams* params_;
    };

    /**
     * Helper class (and thread) that determines the remote 
     * TCP CL version and hands it off to the appropriate 
     * handler on this side.
     */
    class Connection0 : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection0(TCPConvergenceLayer* cl, TCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection0(TCPConvergenceLayer* cl, TCPLinkParams* params,
                   int fd, in_addr_t addr, u_int16_t port);

        /**
         * Destructor.
         */
        virtual ~Connection0();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a) override;

        /**
         * Override StreamConvergenceLayer to send TCPCL4 messages
         */
        virtual void break_contact(ContactEvent::reason_t reason) override;

    protected:

        /// @{ Virtual from CLConnection
        virtual void run() override;
        virtual void connect() override;
        virtual void accept() override;
        virtual void disconnect() override;
        virtual void initialize_pollfds() override;
        virtual void handle_poll_activity() override;
        virtual void handle_contact_initiation() override;
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        virtual void process_data() override;
        virtual void send_data() override {}
        /// @}
        
        /// Hook for handle_poll_activity to receive data
        virtual void recv_data();


        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        TCPLinkParams* tcp_lparams()
        {
            TCPLinkParams* ret = dynamic_cast<TCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TCPClient* sock_ = nullptr;           ///< The socket
        struct pollfd*    sock_pollfd_ =  nullptr;   ///< Poll structure for the socket

        bool contact_initiation_received_ = false;   ///< Whether contact header has been received

        uint8_t peer_tcpcl_version_ = 4;             ///< TCPCL version of peer - which Connection type to instantiate

    };



    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that to deal with NAT, the side
     * which does the active connect is not necessarily the sender.
     */
    class Connection3 : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection3(TCPConvergenceLayer* cl, TCPLinkParams* params,
                    oasys::TCPClient* sock, std::string& nexthop);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection3(TCPConvergenceLayer* cl, TCPLinkParams* params,
                    oasys::TCPClient* sock, std::string& nexthop,
                    oasys::StreamBuffer& initial_recvbuf);

        /**
         * Destructor.
         */
        virtual ~Connection3();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a) override;

    protected:
        friend class TCPConvergenceLayer;

        /// @{ Virtual from CLConnection
        virtual void run() override;
        virtual void connect() override;
        virtual void accept() override;
        virtual void disconnect() override;
        virtual void initialize_pollfds() override;
        virtual void handle_poll_activity() override;
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data() override;
        /// @}
        
        /// Hook for handle_poll_activity to receive data
        void recv_data();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        TCPLinkParams* tcp_lparams()
        {
            TCPLinkParams* ret = dynamic_cast<TCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TCPClient* sock_;	///< The socket
        struct pollfd*	  sock_pollfd_;	///< Poll structure for the socket
    };



    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that to deal with NAT, the side
     * which does the active connect is not necessarily the sender.
     */
    class Connection4 : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection4(TCPConvergenceLayer* cl, TCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection4(TCPConvergenceLayer* cl, TCPLinkParams* params,
                    oasys::TCPClient* sock, std::string& nexthop,
                    oasys::StreamBuffer& initial_recvbuf);

        /**
         * Destructor.
         */
        virtual ~Connection4();

        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a) override;

        /**
         * Override StreamConvergenceLayer to send TCPCL4 messages
         */
        virtual void break_contact(ContactEvent::reason_t reason) override;

    protected:

        /// @{ Virtual from CLConnection
        virtual void run() override;
        virtual void connect() override;
        virtual void accept() override;
        virtual void disconnect() override;
        virtual void initiate_contact() override;
        virtual void handle_contact_initiation() override;
        virtual void initialize_pollfds() override;
        virtual void handle_poll_activity() override;
        /// @}

        virtual void initiate_session_negotiation();
        virtual bool start_tls_server();
        virtual bool start_tls_client();
        virtual bool handle_session_negotiation();


        /// @{ virtual from StreamConvergenceLayer::Connection
        virtual void process_data() override;
        virtual bool handle_data_segment(u_int8_t flags) override;
        virtual bool handle_ack_segment(uint8_t flags) override;
        virtual bool handle_refuse_bundle(uint8_t flags) override;
        virtual bool handle_reject_msg();
        virtual bool handle_shutdown(uint8_t flags) override;


        virtual void send_data() override;
        virtual void send_data_tls();
        virtual void send_keepalive() override;
        virtual bool send_next_segment(InFlightBundle* inflight) override;
        virtual void send_msg_reject(uint8_t msg_type, uint8_t reason);
        virtual void queue_acks_for_incoming_bundle(IncomingBundle* incoming) override;
        /// @}
        
        /// @{ methods active connector uses to delay for peer to announce it
        ///    is a TCPCL3 before continuing with the TCPCL4 hadnshaking
        virtual bool run_delay_for_tcpcl3();
        virtual void handle_poll_activity_while_waiting_for_tcpcl3_contact();
        /// @}


        /// Hook for handle_poll_activity to receive data
        virtual void recv_data();
        virtual void recv_data_tls();

        virtual void refuse_bundle(uint64_t transfer_id, uint8_t reason);

        virtual bool parse_xfer_segment_extension_items(u_char* buf, uint32_t len, 
                                                        bool& critical_item_unhandled,
                                                        uint64_t& extitm_total_len);

        virtual uint32_t encode_transfer_length_extension(u_char* buf, uint32_t len, 
                                                          uint64_t transfer_length);

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        TCPLinkParams* tcp_lparams()
        {
            TCPLinkParams* ret = dynamic_cast<TCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }

    protected:
        oasys::TCPClient* sock_;    ///< The socket
        struct pollfd*      sock_pollfd_;    ///< Poll structure for the socket

        bool contact_initiation_received_ = false;

        bool tls_active_ = false;

#ifdef WOLFSSL_TLS_ENABLED
        WOLFSSL_CTX* wolfssl_ctx_ = nullptr;
        WOLFSSL* wolfssl_handle_ = nullptr;
#endif // WOLFSSL_TLS_ENABLED
    };
};

} // namespace dtn

#endif /* _TCP_CONVERGENCE_LAYER_H_ */
