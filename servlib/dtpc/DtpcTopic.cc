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

#include "DtpcTopic.h"
#include "DtpcTopicStore.h"
#include "DtpcTopicExpirationTimer.h"
#include "DtpcRegistration.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcTopic::DtpcTopic(u_int32_t topic_id)
    : oasys::Logger("DtpcTopic", "/dtpc/topic/%u", topic_id),
      topic_id_(topic_id),
      user_defined_(false),
      description_(""),
      expiration_timer_(NULL),
      registration_(NULL),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false)
{
    collector_list_ = new DtpcTopicCollectorList("collectorlist");
}


//----------------------------------------------------------------------
DtpcTopic::DtpcTopic(const oasys::Builder&)
    : oasys::Logger("DtpcTopic", "/dtpc/topic/bldr"),
      topic_id_(0),
      user_defined_(false),
      description_(""),
      expiration_timer_(NULL),
      registration_(NULL),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false)
{
    collector_list_ = new DtpcTopicCollectorList("collectorlist");
}

//----------------------------------------------------------------------
DtpcTopic::~DtpcTopic () 
{
    oasys::ScopeLock l(&lock_, "destructor");
    if (NULL != expiration_timer_) {
        expiration_timer_->cancel();
        expiration_timer_ = NULL;
    }

    delete collector_list_;
}


//----------------------------------------------------------------------
void
DtpcTopic::serialize(oasys::SerializeAction* a)
{
    a->process("topic_id", &topic_id_);
    a->process("user_defined", &user_defined_);
    a->process("description", &description_);

    collector_list_->serialize(a);
}

//----------------------------------------------------------------------
void
DtpcTopic::format_for_list(oasys::StringBuffer* buf)
{
#define bool_to_str(x)   ((x) ? "true" : "false")

    oasys::ScopeLock l(&lock_, "format_for_list");

    buf->appendf("%7"PRIu32"  %5s %s\n",
                 topic_id_, bool_to_str(user_defined_), description_.c_str());
}

//----------------------------------------------------------------------
void
DtpcTopic::format_verbose(oasys::StringBuffer* buf)
{
#define bool_to_str(x)   ((x) ? "true" : "false")

    oasys::ScopeLock l(&lock_, "format_verbose");

    buf->appendf("topic id %"PRIu32":\n", topic_id_);
    buf->appendf("     user defined: %s\n", bool_to_str(user_defined_));
    buf->appendf("      description: %s\n", description_.c_str());
}

//----------------------------------------------------------------------
void
DtpcTopic::deliver_topic_collector(DtpcTopicCollector* collector)
{
    oasys::ScopeLock l(&lock_, "deliver_topic_collector");

    if (NULL != registration_) {
        log_debug("deliver TopicCollector[%"PRIu32"] to DTPC Registration", 
                  topic_id_);
        registration_->deliver_topic_collector(collector);
    } else {
        log_debug("queue TopicCollector[%"PRIu32"] for next DTPC Registration",
                  topic_id_);
        collector_list_->push_back(collector);

        DtpcTopicStore::instance()->update(this);
    }

    start_expiration_timer();
}

//----------------------------------------------------------------------
void 
DtpcTopic::start_expiration_timer()
{
    if (NULL == expiration_timer_) {
        expiration_timer_ = new DtpcTopicExpirationTimer(topic_id_);

        // arbitrarily chose to check every hour instead of getting elaborate
        struct timeval exp_time;
        gettimeofday(&exp_time, NULL);
        exp_time.tv_sec += 3600;

        expiration_timer_->schedule_at(&exp_time);
    }
}

//----------------------------------------------------------------------
void 
DtpcTopic::remove_expired_items()
{
    size_t size = 0;
    oasys::ScopeLock l(&lock_, "remove_expired_items");

    if (NULL != registration_) {
        registration_->collector_list()->remove_expired_items();
        size = registration_->collector_list()->size();
    } else {
        collector_list_->remove_expired_items();
        size = collector_list_->size();
    }

    // start a new timer if there are data items queued    
    expiration_timer_ = NULL;
    if (size > 0) {
        start_expiration_timer();
    }

    DtpcTopicStore::instance()->update(this);
}

//----------------------------------------------------------------------
void 
DtpcTopic::set_registration(DtpcRegistration* reg)
{
    oasys::ScopeLock l(&lock_, "set_registration");

    if (NULL == reg) {
        if (NULL != registration_) {
            //move the pending data items back to the topic's queue
            registration_->collector_list()->move_contents(collector_list_);
        }
    } else {
        // move all pending data items to the registration queue
        collector_list_->move_contents(reg->collector_list());
    }
    registration_ = reg;

    DtpcTopicStore::instance()->update(this);
}

//----------------------------------------------------------------------
void 
DtpcTopic::set_reloaded_from_ds()
{ 
    reloaded_from_ds_ = true;
    set_in_datastore(true);
    set_queued_for_datastore(true);
}

//----------------------------------------------------------------------
bool 
DtpcTopic::operator==(const DtpcTopic& other) const
{
    return (topic_id_ == other.topic_id_)
        && (description_ == other.description_);
}

} // namespace dtn

#endif // DTPC_ENABLED
