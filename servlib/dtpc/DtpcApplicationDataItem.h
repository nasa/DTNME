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

#ifndef _DTPC_APPLICATION_DATA_ITEM_H_
#define _DTPC_APPLICATION_DATA_ITEM_H_

#include <list>

#include <oasys/serialize/Serialize.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/thread/SpinLock.h>

#include "naming/EndpointID.h"

namespace dtn {

class DtpcApplicationDataItem;

// Definitions for using lists of DTPC ApplicationDataItems
typedef std::list<DtpcApplicationDataItem*>     DtpcApplicationDataItemList;
typedef DtpcApplicationDataItemList::iterator   DtpcApplicationDataItemIterator;


/**
 * DTPC ApplicationDataItem structure which details the type of characteristics 
 * of a transmission profile as defined in 
 */
class DtpcApplicationDataItem: public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    DtpcApplicationDataItem(const EndpointID& remote_eid, 
                            u_int32_t topic_id);

    /**
     * Constructor for deserialization
     */
    DtpcApplicationDataItem(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcApplicationDataItem ();

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);

    /// @{ Accessors
    virtual oasys::Lock&        lock()           { return lock_; }
    virtual u_int32_t           topic_id()       { return topic_id_; }
    virtual const EndpointID&   remote_eid()     { return remote_eid_; }
    virtual size_t              size()           { return size_; }
    virtual const u_int8_t*     data()           { return data_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_topic_id(u_int32_t t)               { topic_id_ = t; }
    virtual void set_remote_eid(const EndpointID& eid)   { remote_eid_.assign(eid); }
    virtual void set_expiration_ts(u_int64_t t)          { expiration_ts_ = t; }
    virtual void set_data(size_t size, u_int8_t* data);
    virtual void reserve(size_t size);
    virtual void write_data(u_int8_t* buf, size_t offset, size_t size);
    /// @}
private:
    /// lock to serialize access
    oasys::SpinLock lock_;

    /// Topic ID in use
    u_int32_t topic_id_;

    /// EID - Remote EID - Source or Destination depending on context
    EndpointID remote_eid_;

    /// Expiration time (seconds since 1/1/1970 - actual time, not a lifetime)
    /// 
    u_int64_t expiration_ts_;

    /// Size of memory allocated for the ADI
    size_t allocated_size_;

    /// Size of this Application Data Item
    size_t size_;

    /// The actual data for this Application Data Item
    u_int8_t* data_;
};

} // namespace dtn

#endif /* _DTPC_APPLICATION_DATA_ITEM_H_ */
