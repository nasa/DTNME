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

#include "bundling/SDNV.h"

#include "DtpcPayloadAggregator.h"
#include "DtpcRegistration.h"
#include "DtpcTopic.h"
#include "DtpcTopicAggregator.h"
#include "DtpcTopicTable.h"
#include "DtpcProtocolDataUnit.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcTopicAggregator::DtpcTopicAggregator(u_int32_t topic_id, DtpcPayloadAggregator* payload_agg)
    : Logger("DtpcTopicAggregator", "/dtpc/topic/agg"),
      topic_id_(topic_id),
      payload_aggregator_(payload_agg),
      size_(0),
      pending_size_delta_(0),
      item_count_(0),
      in_datastore_(false),
      queued_for_datastore_(false)
{}


//----------------------------------------------------------------------
DtpcTopicAggregator::DtpcTopicAggregator(const oasys::Builder&)
    : Logger("DtpcTopicAggregator", "/dtpc/topic/agg"),
      topic_id_(0),
      payload_aggregator_(NULL),
      size_(0),
      pending_size_delta_(0),
      item_count_(0),
      in_datastore_(false),
      queued_for_datastore_(false)
{}

//----------------------------------------------------------------------
DtpcTopicAggregator::~DtpcTopicAggregator () 
{}


//----------------------------------------------------------------------
void
DtpcTopicAggregator::serialize(oasys::SerializeAction* a)
{
    DtpcApplicationDataItem* data_item = NULL;

    a->process("topic_id", &topic_id_);

    a->process("item_count", &item_count_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        for ( size_t ix=0; ix<item_count_; ++ix ) {
            data_item = new DtpcApplicationDataItem(oasys::Builder::builder());
            data_item->serialize(a);
            data_item_list_.push_back(data_item);
        }
    } else {
        DtpcApplicationDataItemIterator iter = data_item_list_.begin();
        while (iter != data_item_list_.end()) {
            data_item = *iter;
            data_item->serialize(a);
            ++iter;
        }
    }
}

//----------------------------------------------------------------------
int64_t
DtpcTopicAggregator::send_data_item(DtpcApplicationDataItem* data_item, bool optimize, 
                                    bool* wait_for_elision_func)
{
    *wait_for_elision_func = false;
    int64_t size_delta = 0;

    oasys::ScopeLock l(&lock_, "send_data_item");


    DtpcTopicTable* toptab = DtpcTopicTable::instance();
    DtpcTopic* topic = toptab->get(data_item->topic_id());
    if (NULL == topic) {
        log_err("send_data_item: DtpcTopic object not found for topic: %"PRIu32,
                data_item->topic_id());
    } else {
        data_item_list_.push_back(data_item);
        ++item_count_;

        DtpcRegistration* dtpc_reg = topic->registration();
        if (optimize && NULL != dtpc_reg && dtpc_reg->has_elision_func()) {
            dtpc_reg->add_topic_aggregator(this);
            *wait_for_elision_func = true;
            pending_size_delta_ = data_item->size();
            // return zero for size_delta since we won't know the size until 
            // the elision function returns its response
        } else {
            size_delta = data_item->size();
            size_ += size_delta;
        }
    }
    
    return size_delta;
}

//----------------------------------------------------------------------
int64_t
DtpcTopicAggregator::elision_func_response(bool modified, 
                                           DtpcApplicationDataItemList* new_data_item_list)
{
    oasys::ScopeLock l(&lock_, "elision_func_response");

    if (modified) {
        int64_t new_size = 0;

        // Delete all of the current data items
        DtpcApplicationDataItem* data_item = NULL;
        DtpcApplicationDataItemIterator del_iter;
        DtpcApplicationDataItemIterator iter = data_item_list_.begin();
        while (iter != data_item_list_.end()) {
            data_item = *iter;
 
            // save this iterator for deletion and bump to the next item
            del_iter = iter;
            ++iter;

            data_item_list_.erase(del_iter);
            delete data_item;
            --item_count_;
        }
        ASSERT(0 == item_count_);

        // Insert the new data items while calculating the new size_
        iter = new_data_item_list->begin();
        while (iter != new_data_item_list->end()) {
            data_item = *iter;
 
            data_item_list_.push_back(data_item);
            ++item_count_;
            new_size += data_item->size();

            // save this iterator for deletion and bump to the next item
            del_iter = iter;
            ++iter;

            new_data_item_list->erase(del_iter);
        }

        // determine and return the delta
        pending_size_delta_ = new_size - size_;
    }

    size_ += pending_size_delta_;
    return pending_size_delta_;
}

//----------------------------------------------------------------------
int64_t
DtpcTopicAggregator::load_payload(DtpcPayloadBuffer* buf)
{
    int64_t topic_size = 0;

    oasys::ScopeLock l(&lock_, "load_payload");

    if (item_count_ > 0) {
        // First field of the Topic Block is the Topic ID (SDNV)
        // reference Table A4-2 Topic Block Fields
        int len = SDNV::encoding_len(topic_id_);
        char* ptr = buf->tail_buf(len);
        len = SDNV::encode(topic_id_, ptr, len);
        ASSERT(len > 0);
        topic_size += len;
        buf->incr_len(len);

        // Next field of the Topic Block is the Payload Record Count
        len = SDNV::encoding_len(data_item_list_.size());
        ptr = buf->tail_buf(len);
        len = SDNV::encode(data_item_list_.size(), ptr, len);
        ASSERT(len > 0);
        topic_size += len;
        buf->incr_len(len);

        // Now loop through the data items adding each to the payload
        DtpcApplicationDataItem* data_item = NULL;
        DtpcApplicationDataItemIterator del_iter;
        DtpcApplicationDataItemIterator iter = data_item_list_.begin();
        while (iter != data_item_list_.end()) {
            data_item = *iter;
 
            // First field of each Data Item Block is the size (SDNV)
            // reference Table A4-3 Payload Record Fields
            len = SDNV::encoding_len(data_item->size());
            ptr = buf->tail_buf(len);
            len = SDNV::encode(data_item->size(), ptr, len);
            ASSERT(len > 0);
            topic_size += len;
            buf->incr_len(len);

            // Next field is the actual data item data
            len = data_item->size();
            ptr = buf->tail_buf(len);
            memcpy(ptr, data_item->data(), len);
            topic_size += len;
            buf->incr_len(len);

            // save this iterator for deletion and bump to the next item
            del_iter = iter;
            ++iter;

            data_item_list_.erase(del_iter);
            delete data_item;
            --item_count_;
        }
    }

    // cleared all data items
    ASSERT(0 == item_count_);
    size_ = 0;

    return topic_size;
}



} // namespace dtn
 
#endif // DTPC_ENABLED
