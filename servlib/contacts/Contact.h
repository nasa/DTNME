/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _CONTACT_H_
#define _CONTACT_H_

#include <memory>

#include <third_party/oasys/debug/DebugUtils.h>
#include <third_party/oasys/debug/Formatter.h>
#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/util/Ref.h>
#include <third_party/oasys/util/RefCountedObject.h>
#include <third_party/oasys/util/Time.h>

namespace dtn {

class Bundle;
class ConvergenceLayer;
class CLInfo;
class Link;

// re-defined from ConvergenceLayer.h
typedef std::shared_ptr<CLInfo> SPtr_CLInfo;


// re-defined from Link.h
typedef oasys::Ref<Link> LinkRef;

/**
 * Encapsulation of an active connection to a next-hop DTN contact.
 * This is basically a repository for any state about the contact
 * opportunity including start time, estimations for bandwidth or
 * latency, etc.
 *
 * It also contains the CLInfo slot for the convergence layer to put
 * any state associated with the active connection.
 *
 * Since the contact object may be used by multiple threads in the
 * case of a connection-oriented convergence layer, and because the
 * object is intended to be deleted when the contact opportunity ends,
 * all object instances are reference counted and will be deleted when
 * the last reference is removed.
 */
class Contact : public oasys::RefCountedObject,
                public oasys::Logger,
                public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    Contact(const LinkRef& link);

private:
    /**
     * Destructor -- private since the class is reference counted and
     * therefore is never explicitly deleted.
     */ 
    virtual ~Contact();
    friend class oasys::RefCountedObject;

public:
    /**
     * Store the convergence layer state associated with the contact.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }
    
    /**
     * Accessor to the convergence layer info.
     */
    CLInfo* cl_info() { return cl_info_; }


    /**
     * Store the convergence layer state associated with the contact as a shared pointer.
     */
    void set_sptr_cl_info(SPtr_CLInfo sptr_cl_info)
    {
        ASSERT((sptr_cl_info_ == NULL && sptr_cl_info != NULL) ||
               (sptr_cl_info_ != NULL && sptr_cl_info == NULL));
        
        sptr_cl_info_ = sptr_cl_info;
    }
    
    /**
     * Clear the shared pointer to the convergence layer state associated with the contact.
     */
    void release_sptr_cl_info()
    {
        sptr_cl_info_.reset();
    }
    
    /**
     * Accessor to the convergence layer info shared pointer variant
     */
    SPtr_CLInfo sptr_cl_info() { return sptr_cl_info_; }




    /**
     * retrieve the incoming list size from the cla via CLInfo
     */
    void get_cla_stats(oasys::StringBuffer& buf);

    /**
     * Accessor to the link
     */
    const LinkRef& link() { return link_; }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz) const;

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize( oasys::SerializeAction *a );


    /// @{ Accessors
    oasys::SpinLock*   lock()             { return &lock_; }

    const oasys::Time& start_time() const { return start_time_; }
    u_int32_t          duration()   const { return duration_; }
    u_int32_t          bps()        const { return bps_; }
    u_int32_t          latency()    const { return latency_; }

    void set_start_time(const oasys::Time& t) { start_time_ = t; }
    void set_duration(u_int32_t duration)     { duration_ = duration; }
    void set_bps(u_int32_t bps)               { bps_ = bps; }
    void set_latency(u_int32_t latency)       { latency_ = latency; }
    /// @}
    
protected:
    /// Time when the contact begin
    oasys::Time start_time_;

    /// Contact duration (0 if unknown)
    u_int32_t duration_;

    /// Approximate bandwidth
    u_int32_t bps_;
    
    /// Approximate latency
    u_int32_t latency_;

    LinkRef link_ ; 	///< Parent link on which this contact exists
    
    CLInfo* cl_info_;	///< convergence layer specific info

    SPtr_CLInfo sptr_cl_info_;  ///< shared pointer to convergence layer specific info (only one or the other should be used)

    oasys::SpinLock lock_; ///< lock to synchronize access to the Contact Object 
                           ///< and hopefully prevent the cl_info_ from being deleted while in use
};

/**
 * Typedef for a reference on a contact.
 */
typedef oasys::Ref<Contact> ContactRef;

} // namespace dtn

#endif /* _CONTACT_H_ */
