/*
 *    Copyright 2009-2010 Darren Long, darren.long@mac.com
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

#ifndef _SEQPACKET_CONVERGENCE_LAYER_H_
#define _SEQPACKET_CONVERGENCE_LAYER_H_

#include "ConnectionConvergenceLayer.h"
#include "CLConnection.h"

namespace dtn {

/**
 * Another shared-implementation convergence layer class for use with
 * reliable, in-order SEQ_PACKET protocols, i.e. Connected Mode AX.25.
 * The goal is to share as much functionality as
 * possible between protocols that have in-order, reliable, delivery
 * semantics, but require fixed sized payloads rather than conforming to 
 * stream semantics.
 *
 * For the protocol, bundles are broken up into configurable-sized
 * segments that are sent sequentially. Only a single bundle is
 * inflight on the wire at one time (i.e. we don't interleave segments
 * from different bundles). When segment acknowledgements are enabled
 * (the default behavior), the receiving node sends an acknowledgement
 * for each segment of the bundle that was received.
 *
 * Keepalive messages are sent back and forth to ensure that the
 * connection remains open. In the case of on demand links, a
 * configurable idle timer is maintained to close the link when no
 * bundle traffic has been sent or received. Links that are expected
 * to be open but have broken due to underlying network conditions
 * (i.e. always on and on demand links) are reopened by a timer that
 * is managed by the contact manager.
 *
 * Flow control is managed through the poll callbacks given by the
 * base class CLConnection. In send_pending_data, we check if there
 * are any acks that need to be sent, then check if there are bundle
 * segments to be sent (i.e. acks are given priority). The only
 * exception to this is that the connection might be write blocked in
 * the middle of sending a data segment. In that case, we must first
 * finish transmitting the current segment before sending any other
 * acks (or the shutdown message), otherwise those messages will be
 * consumed as part of the payload.
 *
 * To make sure that we don't deadlock with the other side, we always
 * drain any data that is ready on the channel. All incoming messages
 * mark state in the appropriate data structures (i.e. InFlightList
 * and IncomingList), then rely on send_pending_data to send the
 * appropriate responses.
 *
 * The InflightBundle is used to record state about bundle
 * transmissions. To record the segments that have been sent, we fill
 * in the sent_data_ sparse bitmap with the range of bytes as we send
 * segments out. As acks arrive, we extend the ack_data_ field to
 * match. Once the whole bundle is acked, the entry is removed from
 * the InFlightList.
 *
 * The IncomingBundle is used to record state about bundle reception.
 * The rcvd_data_ bitmap is extended contiguously with the amount of
 * data that has been received, including partially received segments.
 *
 * To track segments that we have received but haven't yet acked, we
 * set a single bit for the offset of the end of the segment in the
 * ack_data_ bitmap. We also separately record the total range of acks
 * that have been previously sent in acked_length_. As we send acks
 * out, we clear away the bits in ack_data_
 */
class SeqpacketConvergenceLayer : public ConnectionConvergenceLayer {
public:
    /**
     * Constructor
     */
    SeqpacketConvergenceLayer(const char* logpath, const char* cl_name,
                           u_int8_t cl_version);

protected:
    /**
     * Values for ContactHeader flags.
     */
    typedef enum {
        SEGMENT_ACK_ENABLED   = 1 << 0, ///< segment acks requested
        REACTIVE_FRAG_ENABLED = 1 << 1, ///< reactive fragmentation enabled
        NEGATIVE_ACK_ENABLED  = 1 << 2, ///< refuse bundle enabled
    } contact_header_flags_t;

    /**
     * Contact initiation header. Sent at the beginning of a contact. 
     */
    struct ContactHeader {
        u_int32_t magic;        ///< magic word (MAGIC: "dtn!")
        u_int8_t  version;      ///< cl protocol version
        u_int8_t  flags;        ///< connection flags (see above)
        u_int16_t keepalive_interval;   ///< seconds between keepalive packets
        // SDNV   local_eid_length           local eid length
        // byte[] local_eid                  local eid data
    } __attribute__((packed));

    /**
     * Valid type codes for the protocol messages, shifted into the
     * high-order four bits of the byte. The lower four bits are used
     * for per-message flags, defined below.
     */
    typedef enum {
        DATA_SEGMENT    = 0x1 << 4, ///< a segment of bundle data
                                    ///< (followed by a SDNV segment length)
        ACK_SEGMENT     = 0x2 << 4, ///< acknowledgement of a segment
                                    ///< (followed by a SDNV ack length)
        REFUSE_BUNDLE   = 0x3 << 4, ///< reject reception of current bundle
        KEEPALIVE       = 0x4 << 4, ///< keepalive packet
        SHUTDOWN        = 0x5 << 4, ///< about to shutdown
    } msg_type_t;

    /**
     * Valid flags for the DATA_SEGMENT message.
     */
    typedef enum {
        BUNDLE_START    = 0x1 << 1, ///< First segment of a bundle
        BUNDLE_END      = 0x1 << 0, ///< Last segment of a bundle
    } data_segment_flags_t;
    
    /**
     * Valid flags for the SHUTDOWN message.
     */
    typedef enum {
        SHUTDOWN_HAS_REASON = 0x1 << 1, ///< Has reason code
        SHUTDOWN_HAS_DELAY  = 0x1 << 0, ///< Has reconnect delay
    } shutdown_flags_t;
    
    /**
     * Values for the SHUTDOWN reason codes
     */
    typedef enum {
        SHUTDOWN_NO_REASON          = 0xff, ///< no reason code (never sent)
        SHUTDOWN_IDLE_TIMEOUT       = 0x0,  ///< idle connection
        SHUTDOWN_VERSION_MISMATCH   = 0x1,  ///< version mismatch
        SHUTDOWN_BUSY               = 0x2,  ///< node is busy
    } shutdown_reason_t;

    /**
     * Convert a reason code to a string.
     */
    static const char* shutdown_reason_to_str(shutdown_reason_t reason)
    {
        switch (reason) {
        case SHUTDOWN_NO_REASON:    return "no reason";
        case SHUTDOWN_IDLE_TIMEOUT:     return "idle connection";
        case SHUTDOWN_VERSION_MISMATCH: return "version mismatch";
        case SHUTDOWN_BUSY:         return "node is busy";
        }
        NOTREACHED;
    }
    
    /**
     * Link parameters shared among all Seqpacket based convergence layers.
     */
    class SeqpacketLinkParams : public ConnectionConvergenceLayer::LinkParams {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize( oasys::SerializeAction *);

        bool  segment_ack_enabled_; ///< Use per-segment acks
        bool  negative_ack_enabled_;///< Enable negative acks
        u_int keepalive_interval_;  ///< Seconds between keepalive packets
        u_int segment_length_;      ///< Maximum size of transmitted segments
        u_int ack_window_;          ///< Number of segments before acking

    protected:
        // See comment in LinkParams for why this should be protected
        SeqpacketLinkParams(bool init_defaults);
    };
    
    /**
     * Version of the actual CL protocol.
     */
    u_int8_t cl_version_;

    /**
     * Seqpacket connection class.
     */
    class Connection : public CLConnection {
    public:
        /**
         * Constructor.
         */
        Connection(const char* classname,
                   const char* logpath,
                   SeqpacketConvergenceLayer* cl,
                   SeqpacketLinkParams* params,
                   bool active_connector);

        /// @{ virtual from CLConnection
        bool send_pending_data();
        void handle_bundles_queued();
        void handle_cancel_bundle(Bundle* bundle);
        void handle_poll_timeout();
        void break_contact(ContactEvent::reason_t reason);
        /// @}

    protected:
        /**
         * Hook used to tell the derived CL class to drain data out of
         * the send buffer.
         */
        virtual void send_data() = 0;

        /// @{ utility functions used by derived classes
        void initiate_contact();
        void process_data();
        void check_keepalive();
        /// @}

    private:
        /// @{ utility functions used internally in this class
        void note_data_rcvd();
        void note_data_sent();
        bool send_pending_acks();
        bool start_next_bundle();
        bool send_next_segment(InFlightBundle* inflight);
        bool send_data_todo(InFlightBundle* inflight);
        bool finish_bundle(InFlightBundle* inflight);
        void check_completed(InFlightBundle* inflight);
        void send_keepalive();
        
        void handle_contact_initiation();
        bool handle_data_segment(u_int8_t flags);
        bool handle_data_todo();
        bool handle_ack_segment(u_int8_t flags);
        bool handle_refuse_bundle(u_int8_t flags);
        bool handle_keepalive(u_int8_t flags);
        bool handle_shutdown(u_int8_t flags);
        void check_completed(IncomingBundle* incoming);
        /// @}

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        SeqpacketLinkParams* seqpacket_lparams()
        {
            SeqpacketLinkParams* ret = dynamic_cast<SeqpacketLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
    protected:
        InFlightBundle* current_inflight_; ///< Current bundle that's in flight 
        size_t send_segment_todo_;         ///< Bytes left to send of current segment
        std::queue<u_int> sendbuf_sequence_delimiters_; ///< sendbuf_ may hold many segments 
        size_t recv_segment_todo_;  ///< Bytes left to recv of current segment
        struct timeval data_rcvd_;  ///< Timestamp for idle/keepalive timer
        struct timeval data_sent_;  ///< Timestamp for idle timer
        struct timeval keepalive_sent_; ///< Timestamp for keepalive timer
        bool breaking_contact_;     ///< Bit to catch multiple calls to
                                    ///< break_contact 
        bool contact_initiated_;    ///< bit to prevent certain actions before
                                    ///< contact is initiated
        u_int ack_window_todo_; ///< number of segments left in ack_window
    };

    /// For some gcc variants, this typedef seems to be needed
    typedef ConnectionConvergenceLayer::LinkParams LinkParams;

    /// @{ Virtual from ConvergenceLayer
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    /// @}
    
    /// @{ Virtual from ConnectionConvergenceLayer
    bool parse_link_params(LinkParams* params,
                           int argc, const char** argv,
                           const char** invalidp);
    bool finish_init_link(const LinkRef& link, LinkParams* params);
    /// @}

};

} // namespace dtn

#endif /* _STREAM_CONVERGENCE_LAYER_H_ */
