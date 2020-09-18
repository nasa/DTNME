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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(NORM_ENABLED)

#include "NORMSessionManager.h"

namespace dtn {

template<>
NORMSessionManager* oasys::Singleton<NORMSessionManager, true>::instance_ = NULL;

//----------------------------------------------------------------------
NORMSessionManager::NORMSessionManager()
    : Thread("NORMSessionManager", DELETE_ON_EXIT),
      Logger("NORMSessionManager", "/dtn/cl/norm/NORMSessionManager"),
      norm_instance_(NORM_INSTANCE_INVALID)
{
}

//----------------------------------------------------------------------
NORMSessionManager::~NORMSessionManager() {
    receiver_list_t::iterator iter = receivers_.begin();
    receiver_list_t::iterator end = receivers_.end();
    for (; iter != end; ++iter) {
        NormStopReceiver((*iter).session_);
    }
    receivers_.clear();
    destroy_instance();
}

//----------------------------------------------------------------------
void
NORMSessionManager::init() {
    if (! norm_instance_) {
        norm_instance_ = NormCreateInstance();
        if (norm_instance_ == NORM_INSTANCE_INVALID) {
            PANIC("failed to create NORM protocol engine");
        }
        start();
    }
}

//----------------------------------------------------------------------
void
NORMSessionManager::run() {
    NormEvent event;
    while (NormGetNextEvent(norm_instance_, &event))
    {
        oasys::ScopeLock l(&lock_, "NORMSessionManger::run");

        receiver_list_t::iterator begin = receivers_.begin();
        receiver_list_t::iterator end = receivers_.end();
        receiver_list_t::iterator pos = std::find(begin, end, event.session);
        if (pos == receivers_.end())
            continue;

        switch (event.type)
        {
#define CASE(event_type) \
    case event_type: \
    log_debug(#event_type); \
    break;
            CASE(NORM_RX_OBJECT_NEW)
            CASE(NORM_TX_QUEUE_VACANCY)
            CASE(NORM_TX_QUEUE_EMPTY)
            CASE(NORM_TX_FLUSH_COMPLETED)
            CASE(NORM_TX_WATERMARK_COMPLETED)
            CASE(NORM_TX_OBJECT_SENT)
            CASE(NORM_TX_OBJECT_PURGED)
            CASE(NORM_LOCAL_SENDER_CLOSED)
            CASE(NORM_CC_ACTIVE)
            CASE(NORM_CC_INACTIVE)
            CASE(NORM_REMOTE_SENDER_NEW)
            CASE(NORM_REMOTE_SENDER_ACTIVE)
            CASE(NORM_REMOTE_SENDER_INACTIVE)
            CASE(NORM_REMOTE_SENDER_PURGED)
            CASE(NORM_RX_OBJECT_INFO)
            CASE(NORM_RX_OBJECT_UPDATED)

            case NORM_RX_OBJECT_COMPLETED: {
                 log_debug("NORM_RX_OBJECT_COMPLETED");
                 NormObjectRetain(event.object);
                 break;
            }

            case NORM_TX_RATE_CHANGED: {
            	log_info("Rate changed to %f", NormGetTxRate(event.session));
            	break;
            }

            CASE(NORM_TX_CMD_SENT)
            CASE(NORM_RX_CMD_NEW)
            CASE(NORM_RX_OBJECT_ABORTED)
            CASE(NORM_GRTT_UPDATED)
            CASE(NORM_EVENT_INVALID)
            default:
                break;
#undef CASE
        }

        (*pos).queue_->push_back(event);
    }
}

//----------------------------------------------------------------------
void
NORMSessionManager::register_receiver(const NORMReceiver *receiver)
{
    ASSERT(norm_instance_);
    NORMParameters *params = receiver->link_params_;
    NormSessionHandle session = params->norm_session();

    log_debug("new receiver: session: %p eventq: %p",
              session, receiver->eventq_);

    // configure the receiver
    NormSetTOS(session, params->tos());
    NormSetDefaultRxRobustFactor(session, params->rx_robust_factor());
    NormSetSilentReceiver(session, params->silent());

    // push the new norm receiver onto the list
    receivers_.push_back(NORMNode(session, receiver->eventq_));
    NormStartReceiver(session, params->rx_buf_size());
}

//----------------------------------------------------------------------
void
NORMSessionManager::remove_receiver(const NORMReceiver *receiver)
{
    oasys::ScopeLock l(&lock_, "NORMSessionManger::remove_receiver");

    ASSERT(norm_instance_);
    NORMParameters *params = receiver->link_params_;
    NormSessionHandle session = params->norm_session();

    receiver_list_t::iterator begin = receivers_.begin();
    receiver_list_t::iterator end = receivers_.end();
    receiver_list_t::iterator pos = std::find(begin, end, session);

    ASSERT(pos != receivers_.end())
    log_debug("remove receiver: session: %p eventq: %p",
              session, (*pos).queue_);

    NormEvent stop;
    stop.type = NORM_EVENT_INVALID;
    stop.session = NORM_SESSION_INVALID;
    stop.sender = NORM_NODE_INVALID;
    stop.object = NORM_OBJECT_INVALID;
    (*pos).queue_->push_back(stop);

    NormStopReceiver(session);
    receivers_.erase(pos);
}

//----------------------------------------------------------------------
void
NORMSessionManager::destroy_instance(bool stop_first)
{
    if (norm_instance_) {
        if (stop_first) {
            NormStopInstance(norm_instance_);
        }
        NormDestroyInstance(norm_instance_);
        norm_instance_ = 0;
    }
}

} // namespace dtn
#endif // NORM_ENABLED
