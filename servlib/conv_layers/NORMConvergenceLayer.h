/*
 * Copyright 2008 The MITRE Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The US Government will not be charged any license fee and/or royalties
 * related to this software. Neither name of The MITRE Corporation; nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 */

/*
 * This product includes software written and developed 
 * by Brian Adamson and Joe Macker of the Naval Research 
 * Laboratory (NRL).
 */

#ifndef _NORM_CONVERGENCE_LAYER_H_
#define _NORM_CONVERGENCE_LAYER_H_

#if defined(NORM_ENABLED)

#include <list>
#include <vector>
#include <map>
#include <normApi.h>

#include "contacts/Interface.h"
#include "bundling/Bundle.h"
#include "IPConvergenceLayer.h"

#define KEEPALIVE_STR "normcl!"
#define LINK_DEAD_MULTIPLIER 3

namespace dtn {

class NORMSender;
class NORMReceiver;

/**
 * Tunable parameter class for NORM links.
 * Parameters are stored in each Link's CLInfo slot.
 */
class NORMParameters : public CLInfo {
public:
    /**
     * Type for standard link configurations.
     */
    typedef enum {
        EPLRS4HOP = 1,
        EPLRS1HOP,
    } norm_link_t;

    /**
     * Type to capture various norm session behaviors.
     */
    typedef enum {
        RECEIVE_ONLY,
        NEGATIVE_ACKING,
        POSITIVE_ACKING,
    } session_mode_t;

    static const char *session_mode_to_str(session_mode_t type) {
        switch(type) {
            case RECEIVE_ONLY:          return "receive only";
            case NEGATIVE_ACKING:       return "negative acking";
            case POSITIVE_ACKING:       return "positive acking";
            default:                    return "unknown";
        }
    }

    /**
     * Default port used by the norm cl for multicast.
     * (subject to change)
     **/
    static const u_int16_t NORMCL_DEFAULT_MPORT = 4558;

    /**
     * Default port used by the norm cl for unicast.
     * (subject to change)
     **/
    static const u_int16_t NORMCL_DEFAULT_PORT = 4557;

    /**
     * Parse norm link directives
     */
    static bool parse_link_params(NORMParameters *params,
                                  int argc, const char** argv,
                                  const char** invalidp);

    /**
     * Constructor.
     */
    NORMParameters();

    /**
     * Destructor.
     */
    virtual ~NORMParameters() {}

    /**
     * Copy Constructor.
     */
    NORMParameters(const NORMParameters &params);

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction *a);

    ///@{Setters
    void set_norm_session(NormSessionHandle norm_session)
        {norm_session_ = norm_session;}
    void set_session_mode(session_mode_t mode)
        {norm_session_mode_ = mode;}
    void set_nodeid(in_addr_t addr)
        {nodeid_ = addr;}
    void set_local_port(u_int16_t port)
        {local_port_ = port;}
    void set_group_addr(in_addr_t addr)
        {group_addr_ = addr;}
    void set_remote_addr(in_addr_t addr)
        {remote_addr_ = addr;}
    void set_remote_port(u_int16_t port)
        {remote_port_ = port;}
    void set_multicast_dest(bool mdest)
        {multicast_dest_ = mdest;}
    void set_backoff_factor(double factor)
        {backoff_factor_ = factor;}
    void set_group_size(u_int32_t size)
        {group_size_ = size;}
    void set_acking_list(const char *addr_list)
        {acking_list_.assign(addr_list);}
    void set_norm_sender(NORMSender *sender)
        {norm_sender_ = sender;}
    void set_norm_receiver(NORMReceiver *receiver)
        {norm_receiver_ = receiver;}
    ///@}

    ///@{Accessors
    const std::string &multicast_interface()    {return multicast_interface_;}
    const char *multicast_interface_c_str()     {return multicast_interface_.c_str();}
    in_addr_t nodeid()               {return nodeid_;}
    u_int16_t local_port()           {return local_port_;}
    in_addr_t group_addr()           {return group_addr_;}
    in_addr_t remote_addr()          {return remote_addr_;}
    u_int16_t remote_port()          {return remote_port_;}
    bool multicast_dest()            {return multicast_dest_;}
    bool cc()                        {return cc_;}
    bool ecn()                       {return ecn_;}
    double rate()                    {return rate_;}
    u_int16_t segment_size()         {return segment_size_;}
    u_int64_t fec_buf_size()         {return fec_buf_size_;}
    u_char block_size()              {return block_size_;}
    u_char num_parity()              {return num_parity_;}
    u_char auto_parity()             {return auto_parity_;}
    double backoff_factor()          {return backoff_factor_;}
    u_int32_t group_size()           {return group_size_;}
    NormSize tx_cache_size_max()     {return tx_cache_size_max_;}
    u_int32_t tx_cache_count_min()   {return tx_cache_count_min_;}
    u_int32_t tx_cache_count_max()   {return tx_cache_count_max_;}
    u_int32_t rx_buf_size()          {return rx_buf_size_;}
    u_int32_t tx_robust_factor()     {return tx_robust_factor_;}
    u_int32_t rx_robust_factor()     {return rx_robust_factor_;}
    u_int32_t keepalive_intvl()      {return keepalive_intvl_;}
    u_int32_t object_size()          {return object_size_;}
    u_int32_t inter_object_pause()   {return inter_object_pause_;}
    u_int8_t tos()                   {return tos_;}
    bool ack()                       {return ack_;}
    const std::string &acking_list() {return acking_list_;}
    bool silent()                    {return silent_;}

    session_mode_t      norm_session_mode() {return norm_session_mode_;}
    NormSessionHandle   norm_session()      {return norm_session_;}
    NORMSender          *norm_sender()      {return norm_sender_;}
    NORMReceiver        *norm_receiver()    {return norm_receiver_;}
    ///@}

protected:
    // helper functions for named link configurations
    void fe();
    void eplrs4hop();
    void eplrs1hop();

    // calculate inter-object pause
    void pause_time();

    // misc. parameters
    std::string multicast_interface_;   ///< bind multicast addr to this interface
    in_addr_t nodeid_;                  ///< unique norm node id for this session
    u_int16_t local_port_;              ///< Local session port (norm interfaces)
    in_addr_t group_addr_;              ///< Group address (norm interfaces)
    in_addr_t remote_addr_;             ///< Remote session address
    u_int16_t remote_port_;             ///< Remote session port
    bool cc_;                           ///< Whether congestion control is enabled
    bool ecn_;                          ///< Whether explicit congestion notification is enabled
    u_int16_t segment_size_;            ///< max payload size (bytes) of NORM sender messages
    u_int64_t fec_buf_size_;            ///< max memory space (bytes) for precalculated FEC segments
    u_char block_size_;                 ///< source symbol segments per FEC coding block
    u_char num_parity_;                 ///< maximum parity symbol segments per FEC coding block
    u_char auto_parity_;                ///< accompany NORM_DATA messages with set # of FEC segments
    double backoff_factor_;             ///< Scale timeouts for NACK repair process
    u_int32_t group_size_;              ///< Estimate of receiver group size
    NormSize tx_cache_size_max_;        ///< max total size of enqueued objects allowed
    u_int32_t tx_cache_count_min_;      ///< min # of objects allowed (not limited by tx_cache_size_max)
    u_int32_t tx_cache_count_max_;      ///< max # of objects allowed in tx queue
    u_int32_t rx_buf_size_;             ///< size of object receive buffer
    u_int32_t tx_robust_factor_;        ///< number of flush messages sent at end of tx
    u_int32_t rx_robust_factor_;        ///< number of receiver persistent repair requests before giving up
    u_int32_t keepalive_intvl_;         ///< millisecs between keepalive messages
    u_int8_t tos_;                      ///< diffserv codepoint for the link
    bool ack_;                          ///< whether to collect postive acks on unicast links
    std::string acking_list_;           ///< comma separated list of hostnames
    bool silent_;                       ///< silent receiver operation
    double rate_;                       ///< Transmit rate (in bps)
    u_int32_t object_size_;             ///< norm object size for bundles
    u_int32_t tx_spacer_;               ///< constant milisecs added into calc of inter_object_pause_

    u_int32_t inter_object_pause_;          ///< milisecs to wait between chunk transmissions
    bool multicast_dest_;                   ///< whether the remote address is multicast
    session_mode_t    norm_session_mode_;   ///< norm session behavior
    NormSessionHandle norm_session_;        ///< norm session
    NORMSender     *norm_sender_;           ///< NORMSender instance
    NORMReceiver   *norm_receiver_;         ///< NORMReceiver instance
};

/**
 * A Nack-Oriented Reliable Multicast (NORM) convergence layer.
 * (see doc/norm_conv_layer.txt)
 *
 * Norm sessions persist across link down/up events in order
 * to take full advantage of the built-up tx cache used to
 * satisfy repair requests.
 */
class NORMConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * struct for storing metadata on bundle chunks.
     */
    struct BundleInfo {
        BundleInfo(u_int32_t seconds, u_int32_t seqno,
                   u_int32_t frag_offset, u_int32_t total_length,
                   u_int32_t payload_offset, u_int32_t length,
                   u_int32_t object_size, u_int16_t chunk);

        u_int32_t seconds_;
        u_int32_t seqno_;
        u_int32_t frag_offset_;
        u_int32_t total_length_;
        u_int32_t payload_offset_;
        u_int32_t length_;
        u_int32_t object_size_;
        u_int16_t chunk_;
    };

    /**
     * Default NORM link parameters.
     */
    static NORMParameters defaults_;

    /**
     * Constructor.
     */
    NORMConvergenceLayer();

    /**
     * Destructor.
     */
    virtual ~NORMConvergenceLayer() {}

    /**
     * Create a new LinkParams structure.
     */
    virtual CLInfo* new_link_params();
 
    /*
     * Set default link options.
     */
    virtual bool set_link_defaults(int argc, const char* argv[],
                                   const char** invalidp);

    /**
     * Bring up a new interface.
     */
    virtual bool interface_up(Interface* iface,
                              int argc, const char* argv[]);
    virtual void interface_activate(Interface* iface);
    
    /**
     * Bring down the interface.
     */
    virtual bool interface_down(Interface* iface);

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
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

    /**
     * Try to cancel transmission of a given bundle on the contact.
     */
    void cancel_bundle(const LinkRef& link, const BundleRef &bundle);

    /**
     * Report if the given bundle is queued on the given link.
     */
    bool is_queued(const LinkRef& contact, Bundle* bundle);

    oasys::Lock *lock()                    {return &lock_;}

protected:
    /**
     * Wrapper around Norm API create session call.
     */
    bool create_session(NormSessionHandle *handle, NormNodeId nodeid,
                        in_addr_t addr, u_int16_t port);

    bool multicast_addr(in_addr_t addr);

    void issue_bundle_queued(const LinkRef &link, NORMSender *sender);

    void open_contact_abort(const LinkRef &link,
                            NORMParameters *params = 0,
                            NormSessionHandle session = NORM_SESSION_INVALID);

    mutable oasys::SpinLock lock_;       ///< Lock to protect internal data structures.
};

} // namespace dtn

#endif // NORM_ENABLED
#endif // _NORM_CONVERGENCE_LAYER_H_
