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

#ifndef _NORM_RECEIVER_H_
#define _NORM_RECEIVER_H_

#if defined(NORM_ENABLED)

#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>
#include "NORMSender.h"

namespace dtn {

class NORMParameters;
class ReceiveLoop;

/**
 * NORMConvergenceLayer helper class and thread that
 * listens for incoming NORM engine events.  The behavior of
 * NORMReceiver is varied through the ReceiveLoop instance
 * supplied to the constructor.
 */
class NORMReceiver : public CLInfo,
                     public oasys::Thread,
                     public oasys::Logger
{
public:
    /**
     * Type for a NormEvent message queue.
     */
    typedef oasys::MsgQueue<NormEvent> event_queue_t;

    /**
     * Constructor.
     */
    NORMReceiver(NORMParameters *params, const LinkRef &link,
                 ReceiveLoop *strategy);

    /**
     * Constructor for ReceiveOnly interfaces.
     * Should only be called when setting up norm interfaces.
     */
    NORMReceiver(NORMParameters *params, ReceiveLoop *strategy);

    /**
     * Destructor.
     */
    virtual ~NORMReceiver();

    ///@{ Accessors
    NormSessionHandle norm_session();
    NORMSender *norm_sender();
    NORMParameters *link_params()       {return link_params_;}
    ///@}

protected:
    friend class ReceiveOnly;
    friend class ReceiveWatermark;
    friend class NORMSessionManager;
    friend class NORMConvergenceLayer;

    /**
     * Helper class for timing link inactivity
     */
    class InactivityTimer : public oasys::Timer,
                            public oasys::Logger {
    public:
        InactivityTimer(NORMReceiver *receiver)
        : Logger("NORMReceiver::InactivityTimer",
                 "/dtn/cl/norm/timer"),
                 receiver_(receiver) {}
    
        /**
         * Timer callback function
         */
        void timeout(const struct timeval &now);
    
    protected:
        NORMReceiver *receiver_;
    };

    /**
     * Loop forever calling the process_data function when
     * new data arrives.  Simply calls the strategy run() method.
     */
    virtual void run();

    /**
     * start the InactivityTimer
     * (for opportunistic links)
     */
    void inactivity_timer_start(const LinkRef &link);

    /**
     * Reschedule inactivity timer.
     */
    void inactivity_timer_reschedule();

    BundleRef find_bundle(const BundleTimestamp& creation_ts) const;

    /**
     * Handler to process an arrived bundle.
     */
    void process_data(u_char *bundle_buf, size_t len);

    NORMParameters *link_params_;       ///< pointer to link parameters
    ReceiveLoop *strategy_;             ///< run loop version
    EndpointID remote_eid_;
    InactivityTimer *timer_;            ///< inactivity timer
    mutable oasys::SpinLock lock_;      ///< Lock to protect internal data structures.
    event_queue_t *eventq_;             ///< Message queue for NORM protocol events
};

class ReceiveLoop : public oasys::Logger
{
public:
    /**
     * Destructor.
     */
    virtual ~ReceiveLoop() {}

    /**
     * Loop forever receiving events from the Norm engine.
     */
    virtual void run(NORMReceiver *receiver) = 0;

protected:
    ReceiveLoop();
};

/**
 * For cases where no sender thread exists,
 * (i.e. norm interfaces) or where best-effort
 * service is enabled
 */
class ReceiveOnly : public ReceiveLoop
{
public:
    void run(NORMReceiver *r);
};

/**
 * Receiver run-loop where the corresponding
 * local sender employs watermarks.  Arriving
 * bundle chunks are reassembled with help from
 * norm_info blocks provided by the remote
 * sender.
 */
class ReceiveWatermark : public ReceiveLoop
{
public:
    /**
     * Constructor.
     */
    ReceiveWatermark(SendReliable *send_strategy);

    virtual void run(NORMReceiver *r);

protected:
    static void process_data(NORMReceiver *r, u_char *bundle_buf, size_t len) {
        r->process_data(bundle_buf, len);
    }

    friend class BundleReassemblyBuf;

    class BundleReassemblyBuf : public oasys::Logger {
    public:
        BundleReassemblyBuf(NORMReceiver *receiver);
        ~BundleReassemblyBuf();

        void add_fragment(u_int32_t length, u_int32_t object_size,
                          u_int32_t frag_offset, u_int32_t payload_offset);

        void set(const u_char *buf, size_t length, u_int32_t object_size,
                 u_int32_t frag_offset, u_int16_t chunk,
                 u_int32_t total_length, u_int32_t payload_offset);

        bool check_completes(bool link_up = true);

        u_int16_t num_chunks(u_int32_t length, u_int32_t object_size);

    protected:
        struct Fragment {
            Fragment(u_int32_t length, u_int32_t object_size,
                     u_int32_t frag_offset, u_int32_t payload_offset);
                     
            std::vector<bool> rcvd_chunks_;
            u_char *bundle_;
            u_int32_t length_;
            u_int32_t object_size_;
            u_int32_t frag_offset_;
            u_int32_t payload_offset_;
            bool complete_;
        };
        typedef std::list<Fragment> frag_list_t;

        NORMReceiver *receiver_;
        frag_list_t frag_list_;
    };

    void handle_open_contact(NORMReceiver *receiver, const LinkRef &link);
    void handle_close_contact(NORMReceiver *receiver);

    // Type for a bundle chunk reassembly map.
    typedef std::map<BundleTimestamp, BundleReassemblyBuf*> reassembly_map_t;

    SendReliable *send_strategy_;
    bool link_open_;
    reassembly_map_t reassembly_map_;
};

} // namespace dtn

#endif // NORM_ENABLED
#endif // _NORM_RECEIVER_H_
