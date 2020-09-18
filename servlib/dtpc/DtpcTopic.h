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

#ifndef _DTPC_TOPIC_H_
#define _DTPC_TOPIC_H_

#include <map>

#include <oasys/serialize/Serialize.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/thread/SpinLock.h>

#include "DtpcTopicCollector.h"
#include "DtpcTopicCollectorList.h"

namespace dtn {

class DtpcRegistration;
class DtpcTopic;
class DtpcTopicExpirationTimer;
class DtpcTopicTable;

// Definitions for using maps of DTPC Topics
typedef std::map<u_int32_t, DtpcTopic*>    DtpcTopicMap;
typedef std::pair<u_int32_t, DtpcTopic*>   DtpcTopicPair;
typedef DtpcTopicMap::iterator             DtpcTopicIterator;
typedef std::pair<DtpcTopicIterator, bool> DtpcTopicInsertResult;


/**
 * DTPC Topic structure
 */
class DtpcTopic: public oasys::SerializableObject,
                 public oasys::Logger
{
public:
    /**
     * Constructor
     */
    DtpcTopic(u_int32_t topic_id);

    /**
     * Constructor for deserialization
     */
    DtpcTopic(const oasys::Builder&);

    /**
     * Destructor.
     */
    ~DtpcTopic ();

    /** 
     * Compact format for inclusion in a list print out
     */
    virtual void format_for_list(oasys::StringBuffer* buf);
    
    /** 
     * Detailed print of object
     */
    virtual void format_verbose(oasys::StringBuffer* buf);
    
    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /**
     * Queue Tocpic Collector to be delivered to the registration
     */
    virtual void deliver_topic_collector(DtpcTopicCollector* collector);

    /**
     * Start a timer to invoke the remove_expired_items method
     * (arbitrarily opted to invoke every hour)
     */
    virtual void start_expiration_timer();

    /**
     * Delete any expird Data Items from the queueu
     */
    virtual void remove_expired_items();

    /**
     * Operator overload for comparison
     */
    virtual bool operator==(const DtpcTopic& other) const;
        
    /**
     * Operator overload for comparison
     */
    virtual bool operator!=(const DtpcTopic& other) const { return !operator==(other); }
        
    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual u_int32_t durable_key() { return topic_id_; }

    /// @{ Accessors
    virtual oasys::Lock&        lock()                  { return lock_; }
    virtual u_int32_t           topic_id()              { return topic_id_; }
    virtual bool&               user_defined()          { return user_defined_; }
    virtual const std::string&  description()           { return description_; }
    virtual DtpcRegistration*   registration()          { return registration_; }

    virtual bool                in_datastore()          { return in_datastore_; }
    virtual bool                queued_for_datastore()  { return queued_for_datastore_; }
    virtual bool                reloaded_from_ds()      { return reloaded_from_ds_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_topic_id(u_int32_t t)             { topic_id_ = t; }
    virtual void set_user_defined(bool t)              { user_defined_ = t; }
    virtual void set_description(std::string& t)       { description_ = t; }
    virtual void set_description(const char* t)        { description_ = t; }
    virtual void set_registration(DtpcRegistration* t);

    virtual void set_in_datastore(bool t)              { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t)      { queued_for_datastore_ = t; }
    virtual void set_reloaded_from_ds();
    /// @}
private:
    friend class DtpcTopicTable;

    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique transmission Topic ID
    u_int32_t topic_id_;

    /// Flag indicating whether user defined or on-the-fly defined
    bool user_defined_;

    /// Optional description of the topic
    std::string description_;

    /// Timestamp when next check for expired data items will occur
    DtpcTopicExpirationTimer* expiration_timer_;

    /// Current active API Registration 
    DtpcRegistration* registration_;

    /// List of Topic Collectors to be delivered
    DtpcTopicCollectorList* collector_list_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// Reloaded from the datastore flag
    bool reloaded_from_ds_;
};

} // namespace dtn

#endif /* _DTPC_TOPIC_H_ */
