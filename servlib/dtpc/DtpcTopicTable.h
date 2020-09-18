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
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _DTPC_TOPIC_TABLE_H_
#define _DTPC_TOPIC_TABLE_H_

#include <string>
#include <list>

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringBuffer.h>

#include "DtpcTopic.h"
#include "naming/EndpointID.h"


namespace dtn {

/**
 * Class for the DTPC Topic table.
 */
class DtpcTopicTable : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static DtpcTopicTable* instance() {
        if (instance_ == NULL) {
            instance_ = new DtpcTopicTable();
        }
        return instance_;
    }

    static void shutdown();

    /**
     * Constructor
     */
    DtpcTopicTable();

    /**
     * Destructor
     */
    virtual ~DtpcTopicTable();

    /**
     * Perform processing after storage has been initialized
     */
    virtual void storage_initialized();

    /**
     * Add a new topic to the table. Returns true if the topic
     * is successfully added, false if the topic specification is
     * invalid (or it already exists).
     */
    virtual bool add(oasys::StringBuffer *errmsg, const u_int32_t topic_id, 
                     bool user_defined, const char* description);
    
    /**
     * Add a reloaded topic to the table. Returns true if the topic
     * is successfully added, false if the profile specification is
     * invalid (or it already exists).
     */
    virtual bool add_reloaded_topic(DtpcTopic* topic);
    
    /**
     * Public method to determine if a topic is defined
     */
    virtual DtpcTopic* get(const u_int32_t topic_id);

    /**
     * Remove the specified interface.
     */
    virtual bool del(const u_int32_t topic_id);
    
    /**
     * List the current topics.
     */
    virtual void list(oasys::StringBuffer *buf);

    /**
     * Public method to determine if a topic is defined
     */
    virtual bool find(const u_int32_t topic_id);

    /**
     * Sets registrations to NULL so that all pending ADIs 
     * get returned to the Topic's internal list and written
     * to the data store before shutting down.
     */
    virtual void close_registrations();

protected:
    static DtpcTopicTable* instance_;

    /// Lock to serialize access
    oasys::SpinLock lock_;

    /// Map of Topics using the ID as the key
    DtpcTopicMap topic_list_;

    /// Storage initialized flag
    bool storage_initialized_;

    /**
     * Internal method to find the location of the given topic ID
     */
    virtual bool find(const u_int32_t topic_id, DtpcTopicIterator* iter);
};

} // namespace dtn

#endif /* _DTPC_TOPIC_TABLE_H_ */
