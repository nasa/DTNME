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

#include "bundling/SDNV.h"

#include "DtpcTopicCollector.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcTopicCollector::DtpcTopicCollector(u_int32_t topic_id)
    : DtpcTopicAggregator(topic_id, NULL)
{}


//----------------------------------------------------------------------
DtpcTopicCollector::DtpcTopicCollector(const oasys::Builder& bldr)
    : DtpcTopicAggregator(bldr)
{}

//----------------------------------------------------------------------
DtpcTopicCollector::~DtpcTopicCollector () 
{
    DtpcApplicationDataItem* data_item = NULL;
    DtpcApplicationDataItemIterator iter = data_item_list_.begin();
    while (iter != data_item_list_.end()) {
        data_item = *iter;

        data_item_list_.erase(iter);
        --item_count_;

        delete data_item;

        iter = data_item_list_.begin();
    }
}

//----------------------------------------------------------------------
DtpcApplicationDataItem* 
DtpcTopicCollector::pop_data_item()
{
    DtpcApplicationDataItem* data_item = NULL;

    oasys::ScopeLock l(&lock_, "pop_data_item");

    if (item_count_ > 0) {
        DtpcApplicationDataItemIterator iter = data_item_list_.begin();
        if (iter != data_item_list_.end()) {
            data_item = *iter;
 
            data_item_list_.erase(iter);
            --item_count_;
        }
    }

    return data_item;
}

//----------------------------------------------------------------------
void
DtpcTopicCollector::serialize(oasys::SerializeAction* a)
{
    DtpcTopicAggregator::serialize(a);

    a->process("remote_eid_", &remote_eid_);
    a->process("expireation_ts", &expiration_ts_);
}

//----------------------------------------------------------------------
void
DtpcTopicCollector::deliver_data_item(DtpcApplicationDataItem* data_item)
{
    oasys::ScopeLock l(&lock_, "deliver_data_item");

    data_item_list_.push_back(data_item);
    ++item_count_;
}


//----------------------------------------------------------------------
bool
DtpcTopicCollector::expired()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec >= expiration_ts_);
}

} // namespace dtn

#endif // DTPC_ENABLED
