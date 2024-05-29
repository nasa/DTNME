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

#include "DtpcApplicationDataItem.h"

#include "bundling/BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcApplicationDataItem::DtpcApplicationDataItem(const SPtr_EID& remote_eid, 
                                                 u_int32_t topic_id)
    : topic_id_(topic_id),
      expiration_ts_(0),
      allocated_size_(0),
      size_(0),
      data_(NULL)
{
    set_remote_eid(remote_eid);
}


//----------------------------------------------------------------------
DtpcApplicationDataItem::DtpcApplicationDataItem(const oasys::Builder&)
    : topic_id_(0),
      expiration_ts_(0),
      allocated_size_(0),
      size_(0),
      data_(NULL)
{
    sptr_remote_eid_ = BD_MAKE_EID_NULL();
}

//----------------------------------------------------------------------
DtpcApplicationDataItem::~DtpcApplicationDataItem () 
{
    if ( NULL != data_ ) {
        free(data_);
    }
}


//----------------------------------------------------------------------
void
DtpcApplicationDataItem::serialize(oasys::SerializeAction* a)
{
    size_t sz = size_;
    
    a->process("size", &sz);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        if (sz > allocated_size_) {
            u_int8_t* tmp = (u_int8_t*) realloc(data_, sz);
            if (tmp != nullptr) {
                data_ = tmp;
                allocated_size_ = sz;
            } else {
                log_err_p("/dtpc", "DtpcApplicationDataItem::serialize - Error in realloc of size: %zu", sz);
                return; 
            }
        }
        size_ = sz;
    }
    a->process("data", (u_char*)data_, sz);
}

//----------------------------------------------------------------------
void
DtpcApplicationDataItem::set_data(size_t size, u_int8_t* data)
{
    if (size > allocated_size_) {
        u_int8_t* tmp = (u_int8_t*) realloc(data_, size);
        if (tmp != nullptr) {
            data_ = tmp;
            allocated_size_ = size;
        } else {
            log_err_p("/dtpc", "DtpcApplicationDataItem::set_data - Error in realloc of size: %zu", size);
            return;
        }
    }
    size_ = size;
    memcpy(data_, data, size_);
}

//----------------------------------------------------------------------
void 
DtpcApplicationDataItem::reserve(size_t size)
{
    if (size > allocated_size_) {
        u_int8_t* tmp = (u_int8_t*) realloc(data_, size);
        if (tmp != nullptr) {
            data_ = tmp;
            allocated_size_ = size;
        } else {
            log_err_p("/dtpc", "DtpcApplicationDataItem::set_data - Error in realloc of size: %zu", size);
            return;
        }
    }
    size_ = size;
}

//----------------------------------------------------------------------
void 
DtpcApplicationDataItem::write_data(u_int8_t* buf, size_t offset, size_t size)
{
    ASSERT((offset + size) <= size_);
    memcpy(&data_[offset], buf, size);
}

} // namespace dtn

#endif // DTPC_ENABLED
