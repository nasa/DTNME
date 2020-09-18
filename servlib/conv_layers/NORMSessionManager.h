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

#ifndef _NORM_SESSION_MANAGER_H_
#define _NORM_SESSION_MANAGER_H_

#if defined(NORM_ENABLED)

#include <list>

#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/Singleton.h>

#include "NORMConvergenceLayer.h"
#include "NORMReceiver.h"

namespace dtn {

/**
 * Class (and thread) to manage interactions with Norm instance.
 */
class NORMSessionManager : public oasys::Thread,
                           public oasys::Logger,
                           public oasys::Singleton<NORMSessionManager> {
public:
    /**
     * Destructor.
     */
    virtual ~NORMSessionManager();

    /**
     * Initialize Norm engine.
     */
    void init();

    /**
     * Register a Norm session with the manager.
     */
    void register_receiver(const NORMReceiver *receiver);

    /**
     * Unregister a Norm session.
     */
    //void remove_receiver(const NORMReceiverRef &receiver);
    void remove_receiver(const NORMReceiver *receiver);

    /**
     * Norm instance handle accessor.
     */
    NormInstanceHandle norm_instance() {return norm_instance_;}

protected:
    /**
     * Struct to store info on registered Norm sessions.
     */
    struct NORMNode {
        NORMNode(NormSessionHandle session,
                 oasys::MsgQueue<NormEvent> *queue)
            : session_(session), queue_(queue) {}

        bool operator==(const NormSessionHandle &session) const
        {
            return (session_ == session);
        }

        NormSessionHandle session_;
        oasys::MsgQueue<NormEvent> *queue_;
    };
    typedef std::list<NORMNode> receiver_list_t;

    /**
     * Type for an iterator.
     */
    typedef receiver_list_t::iterator iterator;

    iterator begin() {return receivers_.begin();}

    iterator end() {return receivers_.end();}

    /**
     * Loop on NormGetNextEvent.
     */
    virtual void run();

    /**
     * Orderly shutdown of Norm instance.
     */
    void destroy_instance(bool stop_first = true);

    NormInstanceHandle norm_instance_; ///< Norm istance handle
    mutable oasys::SpinLock lock_;     ///< Lock to protect internal data structures.
    receiver_list_t receivers_;        ///< registered Norm sessions

private:
    friend class oasys::Singleton<NORMSessionManager, true>;
    NORMSessionManager();
};

} // namespace dtn
#endif // NORM_ENABLED
#endif // _NORM_SESSION_MANAGER_H_
