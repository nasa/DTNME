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

#ifndef _DTPC_PROFILE_H_
#define _DTPC_PROFILE_H_

#include <map>

#include <oasys/serialize/Serialize.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/thread/SpinLock.h>

#include "naming/EndpointID.h"

namespace dtn {

class DtpcProfile;
class DtpcProfileTable;

// Definitions for using maps of DTPC Profiles
typedef std::map<u_int32_t, DtpcProfile*>    DtpcProfileMap;
typedef std::pair<u_int32_t, DtpcProfile*>   DtpcProfilePair;
typedef DtpcProfileMap::iterator             DtpcProfileIterator;
typedef std::pair<DtpcProfileIterator, bool> DtpcProfileInsertResult;


/**
 * DTPC Profile structure which details the type of characteristics 
 * of a transmission profile as defined in 
 */
class DtpcProfile: public oasys::SerializableObject
{
public:
    /**
     * Constructor
     */
    DtpcProfile(u_int32_t profile_id);

    /**
     * Constructor for deserialization
     */
    DtpcProfile(const oasys::Builder&);

    /**
     * Destructor.
     */
    virtual ~DtpcProfile ();

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
     * calculate the time of the next retransmit attempt
     */
    virtual struct timeval calc_retransmit_time();

    /**
     * Operator overload for comparison
     */
    virtual bool operator==(const DtpcProfile& other) const;
        
    /**
     * Operator overload for comparison
     */
    virtual bool operator!=(const DtpcProfile& other) const { return !operator==(other); }
        
    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    virtual u_int32_t durable_key() { return profile_id_; }


    /// @{ Accessors
    virtual oasys::Lock&        lock()                   { return lock_; }
    virtual u_int32_t           profile_id()             { return profile_id_; }
    virtual bool                custody_transfer()       { return custody_transfer_; }
    virtual u_int64_t           expiration()             { return expiration_; }
    virtual const EndpointID&   replyto()                { return replyto_; }
    virtual u_int8_t            priority()               { return priority_; }        // class of service
    virtual const char*         priority_str();
    virtual u_int8_t            ecos_ordinal()           { return ecos_ordinal_; }    // extended class of service
    virtual bool                rpt_reception()          { return rpt_reception_; }
    virtual bool                rpt_acceptance()         { return rpt_acceptance_; }
    virtual bool                rpt_forward()            { return rpt_forward_; }
    virtual bool                rpt_delivery()           { return rpt_delivery_; }
    virtual bool                rpt_deletion()           { return rpt_deletion_; }
    virtual int                 retransmission_limit()   { return retransmission_limit_; }
    virtual u_int32_t           aggregation_size_limit() { return aggregation_size_limit_; }
    virtual u_int32_t           aggregation_time_limit() { return aggregation_time_limit_; }
    virtual bool                transport_service()      { return retransmission_limit_ > 0; }
    virtual bool                optimization_service()   { return aggregation_time_limit_ > 0; }

    virtual bool                in_datastore()           { return in_datastore_; }
    virtual bool                queued_for_datastore()   { return queued_for_datastore_; }
    virtual bool                reloaded_from_ds()       { return reloaded_from_ds_; }
    /// @}

    /// @{ Setters and mutable accessors
    virtual void set_profile_id(u_int32_t t)             { profile_id_ = t; }
    virtual void set_custody_transfer(bool t)            { custody_transfer_ = t; }
    virtual void set_expiration(u_int64_t t)             { expiration_ = t; }
    virtual EndpointID* mutable_replyto()                { return &replyto_; }
    virtual void set_priority(u_int8_t t)                { priority_ = t; }
    virtual void set_ecos_ordinal(u_int8_t t)            { ecos_ordinal_ = t; }
    virtual void set_rpt_reception(bool t)               { rpt_reception_ = t; }
    virtual void set_rpt_acceptance(bool t)              { rpt_acceptance_ = t; }
    virtual void set_rpt_forward(bool t)                 { rpt_forward_ = t; }
    virtual void set_rpt_delivery(bool t)                { rpt_delivery_ = t; }
    virtual void set_rpt_deletion(bool t)                { rpt_deletion_ = t; }
    virtual void set_retransmission_limit(int t)         { retransmission_limit_ = t; }
    virtual void set_aggregation_size_limit(u_int32_t t) { aggregation_size_limit_ = t; }
    virtual void set_aggregation_time_limit(u_int32_t t) { aggregation_time_limit_ = t; }

    virtual void set_in_datastore(bool t)                { in_datastore_ = t; }
    virtual void set_queued_for_datastore(bool t)        { queued_for_datastore_ = t; }
    virtual void set_reloaded_from_ds();
    /// @}
private:
    friend class DtpcProfileTable;

    /// lock to serialize access
    oasys::SpinLock lock_;

    /// unique transmission Profile ID
    u_int32_t profile_id_;

    /// Custody transfer service requested flag
    bool custody_transfer_;

    /// Expiration time to be applied to bundles for the profile
    u_int64_t    expiration_;

    /// EID to which receipts/reports are to be sent
    EndpointID   replyto_;

    /// Priority (class of service) for bundles for the profile
    u_int8_t     priority_;

    /// Extended class of service ordinal value for for bundles
    u_int8_t     ecos_ordinal_;

    /// Request reporting of bundle reception
    bool         rpt_reception_;

    /// Request reporting of custody acceptance
    bool         rpt_acceptance_;

    /// Request reporting of bundle forwarding
    bool         rpt_forward_;

    /// Request reporting of bundle delivery
    bool         rpt_delivery_;

    /// Request reporting of bundle deletion
    bool         rpt_deletion_;

    /// Max number of times a data PDU may be retransmitted
    int          retransmission_limit_;

    /// Size threshold for concluding aggregation of a payload
    u_int32_t    aggregation_size_limit_;

    /// Time threshold for concluding aggregation of a payload
    u_int32_t    aggregation_time_limit_;

    /// Is in persistent store
    bool in_datastore_;

    /// Is queued to be put in persistent store
    bool queued_for_datastore_;

    /// Profile reloaded from the datastore flag
    bool reloaded_from_ds_;

};

} // namespace dtn

#endif /* _DTPC_PROFILE_H_ */
