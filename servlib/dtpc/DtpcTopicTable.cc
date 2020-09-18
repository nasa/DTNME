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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef DTPC_ENABLED

#ifndef __STDC_FORMAT_MACROS
#    define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "oasys/util/OptParser.h"
#include "oasys/util/StringBuffer.h"

#include "DtpcDaemon.h"
#include "DtpcTopicStore.h"
#include "DtpcTopicTable.h"

namespace dtn {

DtpcTopicTable* DtpcTopicTable::instance_ = NULL;

//----------------------------------------------------------------------
DtpcTopicTable::DtpcTopicTable()
    : Logger("DtpcTopicTable", "/dtpc/topic/table"),
      storage_initialized_(false)
{
}

//----------------------------------------------------------------------
DtpcTopicTable::~DtpcTopicTable()
{
    oasys::ScopeLock l(&lock_, "DtpcTopicTable destructor");

    DtpcTopicIterator iter;
    for(iter = topic_list_.begin();iter != topic_list_.end(); iter++) {
        delete iter->second;
    }

}

//----------------------------------------------------------------------
void 
DtpcTopicTable::shutdown() 
{
    delete instance_;
}

//----------------------------------------------------------------------
bool
DtpcTopicTable::find(const u_int32_t topic_id)
{
    oasys::ScopeLock l(&lock_, "find (public)");

    DtpcTopicIterator iter = topic_list_.find(topic_id);
    return (topic_list_.end() != iter);
}

//----------------------------------------------------------------------
bool
DtpcTopicTable::find(const u_int32_t topic_id,
                       DtpcTopicIterator* iter)
{
    // lock obtained by caller
    *iter = topic_list_.find(topic_id);
    return (topic_list_.end() != *iter);
}

//----------------------------------------------------------------------
bool
DtpcTopicTable::add(oasys::StringBuffer *errmsg, const u_int32_t topic_id, 
                    bool user_defined, const char* description)
{
    DtpcTopicIterator iter;
    
    oasys::ScopeLock l(&lock_, "add");

    if (find(topic_id, &iter)) {
        log_err("attempt to add Topic ID %"PRIu32" which already exists", topic_id);
        if (errmsg) {
            errmsg->appendf("already exists");
        }
        return false;
    }

    if (!user_defined && DtpcDaemon::params_.require_predefined_topics_) {
        log_err("Not configured to allow on the fly Topic ID %"PRIu32" - %s", topic_id, description);
        if (errmsg) {
            errmsg->appendf("Not configured to allow on the fly Topic ID %"PRIu32" - %s", topic_id, description);
        }
        return false;
    }
    
    DtpcTopic* topic = new DtpcTopic(topic_id);
    topic->set_user_defined(user_defined);
    topic->set_description(description);

    log_info("adding Topic ID %"PRIu32" user defined: %s desc: %s", 
             topic_id, (user_defined?"true":"false"), description);

    topic_list_.insert(DtpcTopicPair(topic_id, topic));

    if (storage_initialized_) {
        log_info("adding topic %"PRIu32" to TopicTable", topic_id);
        topic->set_queued_for_datastore(true);
        DtpcTopicStore::instance()->add(topic);
        topic->set_in_datastore(true);
    }

    return true;
}

//----------------------------------------------------------------------
bool
DtpcTopicTable::add_reloaded_topic(DtpcTopic* topic)
{
    u_int32_t topic_id = topic->topic_id();

    DtpcTopicIterator iter;
    
    oasys::ScopeLock l(&lock_, "add_reloaded_topic");

    if (find(topic_id, &iter)) {
        DtpcTopic* found_topic = iter->second;
        if (*found_topic != *topic) {
            oasys::StringBuffer buf;
            buf.appendf("Reloaded topic (%"PRIu32") does not match definition in configuration file (using config):\n", 
                     topic->topic_id());
            buf.appendf("TopicID UserDef Description\n");
            buf.appendf("------- ------- -----------------------------\n");
            buf.appendf("DataStore: ");
            topic->format_for_list(&buf);
            buf.appendf("ConfgFile: ");
            found_topic->format_for_list(&buf);
            log_multiline(oasys::LOG_WARN, buf.c_str());
        } else {
            log_debug("Reloaded topic (%"PRIu32") matches definition in configuration file", 
                     topic->topic_id());
        }

        // keep the relaoded topic because it may have saved ADIs in its list
        topic_list_.erase(iter);
        topic_list_.insert(DtpcTopicPair(topic_id, topic));
        topic->set_reloaded_from_ds();

        // delete the extra object
        delete found_topic;

        return false;
    }
    
    log_info("adding reloaded topic %"PRIu32, topic_id);

    topic_list_.insert(DtpcTopicPair(topic_id, topic));

    // queue an event to purge any expired ADIs
    DtpcDaemon::post_at_head(new DtpcTopicExpirationCheckEvent(topic_id));

    return true;
}

//----------------------------------------------------------------------
DtpcTopic*
DtpcTopicTable::get(const u_int32_t topic_id)
{
    DtpcTopicIterator iter;

    oasys::ScopeLock l(&lock_, "Dget");

    if (! find(topic_id, &iter)) {
        return NULL;
    } else {
        return iter->second;
    }
}

//----------------------------------------------------------------------
bool
DtpcTopicTable::del(const u_int32_t topic_id)
{
    DtpcTopicIterator iter;
    DtpcTopic* topic;
    
    log_info("removing topic %"PRIu32, topic_id);

    oasys::ScopeLock l(&lock_, "del");

    if (! find(topic_id, &iter)) {
        log_err("error removing topic %"PRIu32": not in DtpcTopicTable",
                topic_id);
        return false;
    }


    //XXX/dz Spec says all Profiles and Topics are supposed to be predefined
    //       so there should not be any deletion but what to do if it is in use??

    topic = iter->second;
    topic_list_.erase(iter);

    if (storage_initialized_) {
        DtpcTopicStore::instance()->del(topic_id);
    }

    delete topic;
    return true;
}

//----------------------------------------------------------------------
void
DtpcTopicTable::list(oasys::StringBuffer *buf)
{
    DtpcTopicIterator iter;
    DtpcTopic* topic;

    oasys::ScopeLock l(&lock_, "list");

    if (1 == topic_list_.size())
        buf->appendf("Topic Table (1 entry):\n");
    else
      buf->appendf("Topic Table (%zu entries):\n", topic_list_.size());

    buf->appendf("TopicID UserDef Description\n");
    buf->appendf("------- ------- -----------------------------\n");

    for (iter = topic_list_.begin(); iter != topic_list_.end(); ++(iter)) {
        topic = iter->second;
        topic->format_for_list(buf);
    }
}

//----------------------------------------------------------------------
void
DtpcTopicTable::storage_initialized()
{
    storage_initialized_ = true;

    // loop through and add any new topics to storage
    DtpcTopicIterator iter;
    DtpcTopic* topic;

    oasys::ScopeLock l(&lock_, "storage_initialized");

    for (iter = topic_list_.begin(); iter != topic_list_.end(); ++(iter)) {
        topic = iter->second;
        if (!topic->queued_for_datastore()) {
            log_info("adding topic %"PRIu32" to TopicTable", topic->topic_id());
            topic->set_queued_for_datastore(true);
            DtpcTopicStore::instance()->add(topic);
            topic->set_in_datastore(true);
        }
    }
}

//----------------------------------------------------------------------
void
DtpcTopicTable::close_registrations()
{
    DtpcTopicIterator iter;
    DtpcTopic* topic;

    oasys::ScopeLock l(&lock_, "close_registrations");

    for (iter = topic_list_.begin(); iter != topic_list_.end(); ++(iter)) {
        topic = iter->second;
        topic->set_registration(NULL);
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
