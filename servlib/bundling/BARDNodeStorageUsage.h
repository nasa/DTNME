/*
 *    Copyright 2021 United States Government as represented by NASA
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

#ifndef _BARD_NODE_STORAGE_USAGE_H_
#define _BARD_NODE_STORAGE_USAGE_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BARD_ENABLED

#include <map>
#include <memory>

#include <third_party/oasys/serialize/Serialize.h>

#include "BARDRestageCLIF.h"

namespace dtn {




/**
 * The BARDNodeStorageUsage class contains both quota and usage tracking
 * data for a specifc source or destination node. Only the quota variables
 * are serialized for storage in a database table.
 */
class BARDNodeStorageUsage: public oasys::SerializableObject
{
public:

    /**
     * Static function to generate a key string
     * (should only be used for IPN and IMC schemes when you have a numeric representation of the node number)
     *
     * @param quota_type Indicates whether the key if for a Destination or Source Endpoint ID
     * @param naming_shceme Indicates whether the key is for an Endpoint ID of type IPN or IMC
     * @param node_number The IPN or IMC node number
     */
    static std::string make_key(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, size_t node_number);

    /**
     * Static function to generate a key string
     * (can also be used for IPN and IMC schemes if you have the node number as a text representation)
     *
     * @param quota_type Indicates whether the key if for a Destination or Source Endpoint ID
     * @param naming_shceme Indicates whether the key is for an Endpoint ID of type IPN, DTN or IMC
     * @param node_number The DTN node name or IPN/IMC node number as text
     */
    static std::string make_key(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename);

    /**
     * Constructor
     */
    BARDNodeStorageUsage();

    /**
     * Constructor
     */
    BARDNodeStorageUsage(bard_quota_type_t quota_type, 
                         bard_quota_naming_schemes_t naming_scheme,
                         size_t node_number);

    /**
     * Constructor
     */
    BARDNodeStorageUsage(bard_quota_type_t quota_type,
                         bard_quota_naming_schemes_t naming_scheme,
                         std::string& nodename);

    /**
     * Constructor for deserialization
     */
    BARDNodeStorageUsage(const oasys::Builder&);

    /**
     * Destructor
     */
    ~BARDNodeStorageUsage ();

    /**
     * Comparison operator for use in std::map
     */
    bool operator< (const BARDNodeStorageUsage& other);

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    std::string durable_key();

    /**
     * Copy all of the data elements from a provided object
     *
     * @prarm other The other object to copy data from
     */
    void copy_data(BARDNodeStorageUsage* other);

    /**
     * Check whether it is time to start a new email frequency period
     * and clear all of the msg sent flags
     */
    void check_email_frequency_period(size_t freq_period);


    /// @{ Accessors
    std::string                 key()                 { return durable_key(); }
    bard_quota_type_t           quota_type()          { return quota_type_; }
    bool                        quota_type_src();
    const char*                 quota_type_cstr();
    bard_quota_naming_schemes_t naming_scheme()       { return naming_scheme_; }
    const char*                 naming_scheme_cstr();
    size_t                      node_number()         { return node_number_; }
    std::string                 nodename()            { return nodename_; }

    bool                        has_quota()           { return ((quota_internal_bytes_ != 0) || (quota_internal_bundles_ != 0)); }

    bool                        quota_in_datastore()        { return quota_in_datastore_; }
    bool                        quota_modified()            { return quota_modified_; }
    size_t                      quota_internal_bundles()    { return quota_internal_bundles_; }
    size_t                      quota_internal_bytes()      { return quota_internal_bytes_; }
    size_t                      quota_external_bundles()    { return quota_external_bundles_; }
    size_t                      quota_external_bytes()      { return quota_external_bytes_; }
    bool                        quota_refuse_bundle()       { return quota_refuse_bundle_; }
    bool                        quota_auto_reload()         { return quota_auto_reload_; }
    std::string                 quota_restage_link_name()   { return quota_restage_link_name_; }

    size_t                      committed_internal_bundles();       ///< get total inuse and reserved internal bundles
    size_t                      committed_internal_bytes();         ///< get total inuse and reserved internal bytes
    size_t                      committed_external_bundles();       ///< get total inuse and reserved external bundles
    size_t                      committed_external_bytes();         ///< get total inuse and reserved external bytes
    size_t                      last_committed_external_bundles();  ///< get total inuse and reserved external bundles while rescan in progress
    size_t                      last_committed_external_bytes();    ///< get total inuse and reserved external bytes while rescan in progress
    
    /**
     * whether current internal storage is over-quota and by how much
     * @param bundles_over number of bundles over quota
     * @param bytes_over number of bytes over quota
     * @return true of over quota and false if not
     */
    bool                        over_quota(size_t& bundles_over, size_t& bytes_over);
    /// @}



    /// @{ Setters and mutable accessors
    void set_quota_type(bard_quota_type_t t);
    void set_naming_scheme(bard_quota_naming_schemes_t t);
    void set_node_number(size_t   t);
    void set_nodename(std::string& t);

    void set_quota_deleted(bool t);

    void set_quota_in_datastore(bool t)         { quota_in_datastore_  = t; }
    void set_quota_modified(bool t)             { quota_modified_      = t; }
    void set_quota_internal_bundles(size_t   t);
    void set_quota_internal_bytes(size_t   t);
    void set_quota_external_bundles(size_t   t);
    void set_quota_external_bytes(size_t   t);
    void set_quota_refuse_bundle(bool t);
    void set_quota_auto_reload(bool t);
    void clear_quota_restage_link_name();
    void set_quota_restage_link_name(std::string& t);
    /// @}


public:

    // usage elements
    size_t inuse_internal_bundles_       = 0;   ///< current number of bundles in internal storage 
    size_t inuse_internal_bytes_         = 0;   ///< current number of payload bytes in internal storage
    size_t inuse_external_bundles_       = 0;   ///< current number of bundles in external storage 
    size_t inuse_external_bytes_         = 0;   ///< current number of payload bytes in external storage

    size_t reserved_internal_bundles_    = 0;    ///< current number of reserved bundles in internal storage 
    size_t reserved_internal_bytes_      = 0;    ///< current number of reserved payload bytes in internal storage
    size_t reserved_external_bundles_    = 0;    ///< current number of reserved bundles in internal storage 
    size_t reserved_external_bytes_      = 0;    ///< current number of reserved payload bytes in internal storage

    size_t last_inuse_external_bundles_  = 0;    ///< saved number of bundles in external storage to be used while rescan is in progress
    size_t last_inuse_external_bytes_    = 0;    ///< saved byte in external storage to be used while rescan is in progress

    // administrative variable
    time_t last_reload_command_time_     = 0;    ///< last time the BARD issued a reload command for this quota

    // state flags for email notifiations
    bool   internal_threshold_low_reached_  = false;
    bool   internal_threshold_high_reached_ = false;
    bool   internal_quota_reached_          = false;
    bool   external_threshold_low_reached_  = false;
    bool   external_threshold_high_reached_ = false;
    bool   external_quota_reached_          = false;

    /// time stamp when the first message within a frequency time period was sent
    time_t email_freq_period_start_ = 0;

    /// flags indicating whether each type of message has been sent
    bool email_sent_internal_below_low_watermark_ = false;
    bool email_sent_internal_low_watermark_ = false;
    bool email_sent_internal_high_watermark_ = false;
    bool email_sent_internal_quota_reached_ = false;

    bool email_sent_external_below_low_watermark_ = false;
    bool email_sent_external_low_watermark_ = false;
    bool email_sent_external_high_watermark_ = false;
    bool email_sent_external_quota_reached_ = false;


protected:

    // elements for the key
    bool                        quota_in_datastore_  = false;   ///< Whether this is a configured quota and stored in the databse
    bool                        quota_modified_      = false;   ///< Whether the quota information has been modified (database needs update)
    bard_quota_type_t           quota_type_          = BARD_QUOTA_TYPE_DST;          ///< quota type (source or destination endpoint)
    bard_quota_naming_schemes_t naming_scheme_       = BARD_QUOTA_NAMING_SCHEME_IPN; ///< naming scheme identifier
    size_t                      node_number_         = 0;       ///< node number if Endpoint is IPN or IMC
    std::string                 nodename_            = "";      ///< node name if Endpoint is DTN

    bool                        key_initialized_     = false;   ///< whether key string is properly set
    std::string                 key_                 = "";      ///< string representation of a unqiue key for persistent storage


    // quota elements
    size_t            quota_internal_bundles_    = 0;       ///< max number of bundles allowed in internal storage 
    size_t            quota_internal_bytes_      = 0;       ///< max number of payload bytes allowed in internal storage
    size_t            quota_external_bundles_    = 0;       ///< max number of bundles allowed in external storage 
    size_t            quota_external_bytes_      = 0;       ///< max number of payload bytes allowed in external storage
    bool              quota_refuse_bundle_       = false;   ///< whether to refuse bundle if quota would be exceeded
    bool              quota_auto_reload_         = false;   ///< whether to initate reload if storage use falls below 20%
    std::string       quota_restage_link_name_   = "";      ///< name of restage convergence layer instance to use for external storage


};

typedef std::shared_ptr<BARDNodeStorageUsage>             SPtr_BARDNodeStorageUsage;

typedef std::map<std::string, SPtr_BARDNodeStorageUsage>  BARDNodeStorageUsageMap;

} // namespace dtn

#endif   // BARD_ENABLED

#endif /* _BARD_NODE_STORAGE_USAGE_H_ */

