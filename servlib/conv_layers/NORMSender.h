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

#ifndef _NORM_SENDER_H_
#define _NORM_SENDER_H_

#if defined(NORM_ENABLED)

#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>

namespace dtn {

class NORMParameters;
class NORMReceiver;
class SendStrategy;

/*
 * NORMConvergenceLayer helper class and thread that wraps
 * the sender-side per-contact state.  The behavior of NORMSender
 * is varied through the SendStrategy instance supplied
 * to the constructor.
 */
class NORMSender : public CLInfo,
                   public oasys::Thread,
                   public oasys::Logger
{
public:
    /**
     * Destructor.
     */
    virtual ~NORMSender();

    /**
     * Initialize the sender (the "real" constructor).
     */
    virtual bool init();

    ///@{ Accessors
    NORMParameters *link_params() {return link_params_;}
    NormSessionHandle norm_session();
    NORMSender *norm_sender();
    NORMReceiver *norm_receiver();
    const ContactRef &contact() {return contact_;}
    SendStrategy *strategy() {return strategy_;}
    bool contact_up() {return contact_up_;}
    bool closing_session() {return closing_session_;}
    const oasys::Time &bundle_sent_time() {return bundle_sent_;}
    ///@}

protected:
    friend class SendStrategy;
    friend class SendBestEffort;
    friend class SendReliable;
    friend class KeepaliveTimer;
    friend class NORMConvergenceLayer;
    friend class NORMReceiver;

    typedef NORMConvergenceLayer::BundleInfo BundleInfo;

    /**
     * Enum for messages from the daemon thread to the sender
     * thread.
     */
    typedef enum {
        CLMSG_INVALID,
        CLMSG_BUNDLE_QUEUED,
        CLMSG_WATERMARK,
        CLMSG_CANCEL_BUNDLE,
        CLMSG_BREAK_CONTACT,
    } clmsg_t;
    
    /**
     * Message to string conversion.
     */
    const char* clmsg_to_str(clmsg_t type) {
        switch(type) {
        case CLMSG_INVALID:         return "CLMSG_INVALID";
        case CLMSG_BUNDLE_QUEUED:   return "CLMSG_BUNDLE_QUEUED";
        case CLMSG_WATERMARK:       return "CLMSG_WATERMARK";
        case CLMSG_CANCEL_BUNDLE:   return "CLMSG_CANCEL_BUNDLE";
        case CLMSG_BREAK_CONTACT:   return "CLMSG_BREAK_CONTACT";
        default: PANIC("bogus clmsg_t");
        }
    }
    
    /**
     * struct used for messages going from the daemon thread to the
     * sender thread.
     */
    struct CLMsg {
        CLMsg()
            : type_(CLMSG_INVALID),
              bundle_("NORMConvergenceLayer::CLMsg") {}
    
        CLMsg(clmsg_t type)
            : type_(type),
              bundle_("NORMConvergenceLayer::CLMsg") {}
    
        CLMsg(clmsg_t type, const BundleRef& bundle)
            : type_(type),
              bundle_(bundle) {}
    
        clmsg_t   type_;
        BundleRef bundle_;
    };

    /**
     * Type for a command message queue.
     */
    typedef oasys::MsgQueue<CLMsg> command_queue_t;

    /**
     * Helper class for a keepalive mechanism.
     */
    class KeepaliveTimer : public oasys::Timer,
                                  oasys::Logger {
    public:
        KeepaliveTimer(NORMSender *sender)
            : Logger("NORMSender::KeepaliveTimer",
                     "/dtn/cl/norm/timer"),
              sender_(sender) {}

        /**
         * Timer callback function.
         * timeout_bottom_half(), called at the end,
         * may be implemented by SendStrategy concrete
         * classes to carry out periodic tasks.
         */
        void timeout(const struct timeval &now);

    protected:
        NORMSender *sender_;
    };

    /**
     * Constructor.
     */
    NORMSender(NORMParameters *params,
               const ContactRef& contact,
               SendStrategy *strategy);

    /**
     * Main thread loop.
     */
    void run();

    /**
     * Handle bundle queued events.
     */
    void handle_bundle_queued();

    /**
     * Move bundle to inflight queue.
     */
    void move_bundle_to_inflight(BundleRef &bref, size_t length);

    /**
     * Move bundle to link queue.
     */
    void move_bundle_to_link(Bundle *bundle, size_t length);

    /**
     * Enqueue data for transmission.
     */
    NormObjectHandle enqueue_data(Bundle *bundle, BlockInfoVec *blocks,
                                  size_t length, size_t offset, bool *last,
                                  BundleInfo *info = 0, size_t info_length = 0);

    /**
     * Destroy the norm session with the contact.
     * Only called from the destructor.
     */
    void really_close_contact();

    /**
     * Record time of last bundle transmission.
     */
    void set_bundle_sent_time();

    void apply_cc();
    void apply_tos();

    NORMParameters *link_params_;///< pointer to link parameters
    ContactRef contact_;         ///< the contact we're representing
    SendStrategy *strategy_;     ///< send bundles according to this algorithm 
    KeepaliveTimer *timer_;      ///< keepalive timer
    bool contact_up_;
    bool closing_session_;
    oasys::Time bundle_sent_;
    command_queue_t *commandq_;  ///< Message queue for daemon commands
};

class SendStrategy : public oasys::Logger
{
public:
    /**
     * Destructor.
     */
    virtual ~SendStrategy() {}

    /**
     *
     */
    virtual void handle_bundle_queued(NORMSender *sender);


    /**
     * Send one bundle.
     * @return the length of the bundle sent or -1 on error
     */
    virtual void send_bundle(NORMSender *sender, Bundle* bundle,
                             BlockInfoVec *blocks, size_t total_len) = 0;

    /**
     * Cancel transmission of a bundle.
     */
    virtual void handle_cancel_bundle(const LinkRef &link, Bundle *bundle);

    /**
     * Move inflight bundles back to the link queue.
     */
    virtual void handle_close_contact(NORMSender *sender, const LinkRef &link);

    /**
     * Watermarking policy.
     */
    virtual void handle_watermark(NORMSender *sender);

    /**
     * Do additional work at NORMSender keepalive timeout intervals.
     */
    virtual void timeout_bottom_half(NORMSender *sender);

protected:
    SendStrategy();
};

/**
 * Send with flush messages only (i.e. no watermarking,
 * tranmit queue maintenance, or bundle chunking)
 */
class SendBestEffort: public SendStrategy
{
public:
    /**
     * Send one bundle.
     * @return the length of the bundle sent or -1 on error
     */
    virtual void send_bundle(NORMSender *sender, Bundle* bundle,
                             BlockInfoVec *blocks, size_t total_len);
};

/**
 * Send with watermarks 'inline'.  Bundles may be broken into
 * chunks and reassembled on the receiver side.
 */
class SendReliable : public SendStrategy
{
public:
    struct NORMBundle {
        // Constructor.
        NORMBundle(Bundle *bundle, const ContactRef &contact,
                   const LinkRef &link, size_t total_len);

        typedef std::list<NormObjectHandle> handle_list_t;
        typedef handle_list_t::iterator iterator;

        iterator begin() {return handle_list_.begin();}
        iterator end()   {return handle_list_.end();}

        handle_list_t    handle_list_;
        BundleRef        bundle_;
        ContactRef       contact_;
        LinkRef          link_;
        size_t           total_len_;
        bool             sent_;
        iterator         watermark_;
    };

    /**
     * Type for a list of norm objects.
     */
    typedef std::list<NORMBundle*> norm_bundle_list_t;

    /**
     * Type for an iterator.
     */
    typedef norm_bundle_list_t::iterator iterator;

    /**
     * Type for a const iterator.
     */
    typedef norm_bundle_list_t::const_iterator const_iterator;

    /**
     * Constructor.
     */
    SendReliable();

    /**
     * Destructor.
     */
    ~SendReliable();

    /**
     * Iterator used to iterate through the list of norm objects.
     * Iterations _must_ be  completed while holding the list lock,
     * and this method will assert as such.
     */
    iterator begin();

    /**
     * Iterator used to iterate through the list of norm objects.
     * Iterations _must_ be  completed while holding the list lock,
     * and this method will assert as such.
     */
    iterator end();

    /**
     * Const iterator used to iterate through the list of norm objects.
     * Iterations _must_ be  completed while holding the list lock,
     * and this method will assert as such.
     */
    const_iterator begin() const;

    /**
     * Const iterator used to iterate through the list of norm objects.
     * Iterations _must_ be  completed while holding the list lock,
     * and this method will assert as such.
     */
    const_iterator end() const;

    /**
     * Number of bundles in sent_cache_ (unacked).
     */
    size_t size();

    /**
     * Erase one norm object from the sent cache.
     */
    iterator erase(iterator pos);

    /**
     * Erase a range of norm objects from the sent cache.
     */
    iterator erase(iterator first, iterator last);

    void set_watermark_result(NormAckingStatus result)
        {watermark_result_ = result;}

    /**
     *
     */
    virtual void handle_bundle_queued(NORMSender *sender);

    /**
     * Transmit one bundle.
     * @return the length of the bundle sent or -1 on error
     */
    virtual void send_bundle(NORMSender *sender, Bundle* bundle,
                             BlockInfoVec *blocks, size_t total_len);

    /**
     *
     */
    virtual void send_bundle_chunk();

    /**
     *
     */
    virtual void send_bundle_complete(NORMSender *sender,
                                      NORMBundle *norm_bundle);

    /**
     * Emit bundle transmit event and discard corresponding
     * objects from the norm sent cache.
     */
    virtual void bundles_transmitted(NormAckingStatus status);

    /**
     * Cancel transmission of a bundle.
     */
    virtual void handle_cancel_bundle(const LinkRef &link, Bundle *bundle);

    /**
     * Move inflight bundles back to the link queue.
     */
    virtual void handle_close_contact(NORMSender *sender, const LinkRef &link);

    /**
     * Watermarking policy.
     */
    virtual void handle_watermark(NORMSender *sender);

    /**
     * Do additional work at NORMSender keepalive timeout intervals.
     */
    virtual void timeout_bottom_half(NORMSender *sender);

    /**
     * positive ack support
     */
    void push_acking_nodes(NORMSender *sender);

    ///@{ Mutating Accessors
    int8_t &num_tx_pending()                        {return num_tx_pending_;}
    ///@}

    ///@{ Accessors
    NORMBundle *watermark_object()                  {return watermark_object_;}
    NORMBundle *watermark_object_candidate()        {return watermark_object_candidate_;}
    oasys::Notifier *watermark_complete_notifier()  {return watermark_complete_notifier_;}
    oasys::Lock *lock()                             {return &lock_;}
    ///@}

protected:
    struct BundleTransmitState{
        BundleTransmitState(Bundle *bundle, BlockInfoVec *blocks,
                            NORMBundle *norm_bundle,
                            size_t total_len, u_int16_t round,
                            size_t offset, u_int32_t object_size,
                            NORMSender *sender);

        Bundle *bundle_;
        BlockInfoVec *blocks_;
        NORMBundle *norm_bundle_;
        size_t total_len_;
        u_int16_t round_;
        size_t offset_;
        u_int32_t object_size_;
        NORMSender *sender_;
        u_int32_t bytes_sent_;
    };

    BundleTransmitState *bundle_tx_;
    NORMBundle *watermark_object_;                  ///< watermark object tracker
    NORMBundle *watermark_object_candidate_;        ///< watermark candidate object tracker
    NormAckingStatus watermark_result_;
    bool watermark_request_;
    bool watermark_pending_;
    oasys::Notifier *watermark_complete_notifier_;  ///< Notifier for NORM_TX_WATERMARK_COMPLETED events
    int8_t num_tx_pending_;
    norm_bundle_list_t sent_cache_;                 ///< list of sent Norm bundle objects
    mutable oasys::SpinLock lock_;                  ///< Lock to protect internal data structures.
};

} // namespace dtn

#endif // NORM_ENABLED
#endif // _NORM_SENDER_H_
