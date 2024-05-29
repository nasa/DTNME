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
 *    results, designs or products rsulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _BARD_RESTAGE_CL_IF_H_
#define _BARD_RESTAGE_CL_IF_H_


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BARD_ENABLED

#include <map>
#include <memory>
#include <string.h>

#include <third_party/oasys/thread/SpinLock.h>

namespace dtn {


// Data structures shared between the Bundle Restaging Daemon and the Restage Convergence Layer

// forward declaration so we can typecast
class BARDRestageCLIF;
struct RestageCLStatus;

typedef std::shared_ptr<BARDRestageCLIF> SPtr_BARDRestageCLIF;   ///< Shared pointer to a BARDRestageCLIF object

typedef std::shared_ptr<RestageCLStatus> SPtr_RestageCLStatus;   ///< Shared pointer to a RestageCLStatus object

typedef std::map<std::string, SPtr_RestageCLStatus> RestageCLMap;  ///< Definition of a map of RestageCLStatus object indexed by link name
typedef RestageCLMap::iterator RestageCLIterator;                  ///< Definition of an iterator for the RestageCLMap
typedef RestageCLMap::const_iterator RestageCLConstIterator;                  ///< Definition of an iterator for the RestageCLMap


/**
 * Bundle Restaging Daemon Quota types
 */
typedef enum {
    BARD_QUOTA_TYPE_UNDEFINED  = 0,  ///< Undefined quota type
    BARD_QUOTA_TYPE_DST        = 1,  ///< Quota type based on the destination EID of a bundle
    BARD_QUOTA_TYPE_SRC        = 2,  ///< Quota type based on the source EID of a bundle
} bard_quota_type_t;


int bard_quota_type_to_int(bard_quota_type_t quota_type);

const char* bard_quota_type_to_str(bard_quota_type_t quota_type);

bard_quota_type_t int_to_bard_quota_type(int quota_type);

bard_quota_type_t str_to_bard_quota_type(const char* quota_type);


/**
 * Bundle Restaging Daemon Quota Naming Scheme types
 */
typedef enum {
    BARD_QUOTA_NAMING_SCHEME_UNDEFINED   = 0,  ///< Undefined naming scheme
    BARD_QUOTA_NAMING_SCHEME_IPN         = 1,  ///< Quota is for an IPN schme Endpoint ID
    BARD_QUOTA_NAMING_SCHEME_DTN         = 2,  ///< Quota is for a DTN schme Endpoint ID
    BARD_QUOTA_NAMING_SCHEME_IMC         = 3,  ///< Quota is for an IMC schme Endpoint ID
} bard_quota_naming_schemes_t;

int bard_naming_scheme_to_int(bard_quota_naming_schemes_t scheme);

const char* bard_naming_scheme_to_str(bard_quota_naming_schemes_t scheme);

bard_quota_naming_schemes_t int_to_bard_naming_scheme(int scheme);

bard_quota_naming_schemes_t str_to_bard_naming_scheme(const char* scheme);



/**
 * Restage Convergence Layer state codes
 */
typedef enum {
    RESTAGE_CL_STATE_UNDEFINED          = 0,  ///< Undefined state
    RESTAGE_CL_STATE_ONLINE             = 1,  ///< Restage CL is available for use
    RESTAGE_CL_STATE_LOW                = 2,  ///< Restage CL usage crossed 25% threshold
    RESTAGE_CL_STATE_HIGH               = 3,  ///< Restage CL usage crossed 75% threshold
    RESTAGE_CL_STATE_FULL_QUOTA         = 4,  ///< Restage CL is online but quota is fully used
    RESTAGE_CL_STATE_FULL_DISK          = 5,  ///< Restage CL is online but disk is fully used
    RESTAGE_CL_STATE_ERROR              = 6,  ///< Restage CL is online but disk is in an error state and unusable
    RESTAGE_CL_STATE_SHUTDOWN           = 7,  ///< Restage CL instance was shutdown
} restage_cl_states_t;

const char* restage_cl_states_to_str(restage_cl_states_t state);

restage_cl_states_t str_to_restage_cl_state(const char* statet);


/**
 * Structure to maintain state and status information for sharing with the Bundle Restaging Daemon
 */
struct RestageCLStatus {
public:
    SPtr_BARDRestageCLIF sptr_restageclif_;    ///< Shared pointer to the RestageCL ExternalStorageController

    // BARD administrative variables
    time_t      last_error_msg_time_ = 0;     ///< Last time an error message was output (to avoid flodding the log file)


    /// @{ Accessors
    oasys::SpinLock* lock();

    std::string restage_link_name();

    std::string storage_path();
    std::string validated_mount_pt();

    bool        mount_point();
    bool        mount_pt_validated();
    bool        storage_path_exists();

    bool        part_of_pool();
    bool        email_enabled();

    size_t      vol_total_space();
    size_t      vol_space_available();
    bool        disk_space_full();
    size_t      vol_block_size();

    size_t      disk_quota();
    size_t      disk_quota_in_use();
    size_t      disk_num_files();
    bool        disk_quota_full();

    // including these also for now in case they prove useful for BARD reporting
    uint64_t    days_retention();
    bool        expire_bundles();
    uint64_t    ttl_override();
    uint64_t    auto_reload_interval();


    restage_cl_states_t cl_state();
    const char* cl_state_str();
    /// @}


    /// @{ Setters
    void set_restage_link_name(const std::string& restage_link_name);

    void set_storage_path(const std::string& storage_path);
    void set_validated_mount_pt(const std::string& validated_mount_pt);

    void set_mount_point(bool mount_point);
    void set_mount_pt_validated(bool mount_pt_validated);
    void set_storage_path_exists(bool storage_path_exists);

    void set_part_of_pool(bool part_of_pool);
    void set_email_enabled(bool email_enabled);

    void set_vol_total_space(size_t vol_total_space);
    void set_vol_space_available(size_t vol_space_available);
    void inc_vol_space_available(size_t bytes);
    void dec_vol_space_available(size_t bytes);
    void set_disk_space_full(bool disk_space_full);
    void set_vol_block_size(size_t bock_size); ///< set the volume's block size. if zero then will override to 4096.

    void set_disk_quota(size_t disk_quota);
    void set_disk_quota_in_use(size_t disk_quota_in_use);
    void set_disk_num_files(size_t disk_num_files);
    void set_disk_quota_full(bool disk_quota_full);

    // including these also for now in case they prove useful for BARD reporting
    void set_days_retention(uint64_t days_retention);
    void set_expire_bundles(bool expire_bundles);
    void set_ttl_override(uint64_t ttl_override);
    void set_auto_reload_interval(uint64_t auto_reload_interval);


    void set_cl_state(restage_cl_states_t cl_state);

    /// @}

private:
    oasys::SpinLock lock_;                    ///< Lock to serialize access

    std::string restage_link_name_;           ///< Name of the Restage Convergence Layer instance

    std::string storage_path_;                ///< Top level directory to use for external storage
    std::string validated_mount_pt_;          ///< The actual mount point that was validated (vs. the storage path)

    bool        mount_point_ = true;          ///< Whether to verify storage path is mounted
    bool        mount_pt_validated_ = false;  ///< Whether the mount point has been validated as actually mounted
    bool        storage_path_exists_ = false; ///< Whether the storage path was successfully created or already exists

    bool        part_of_pool_ = true;         ///< Whether this instance is part of the BARD pool of storage locations or not
    bool        email_enabled_ = false;       ///< Whether the CL has email notification capability

    size_t      vol_total_space_ = 0;         ///< Total size of the volume (bytes)
    size_t      vol_space_available_ = 0;     ///< Free space available on the volume (bytes)
    size_t      vol_block_size_ = 4096;       ///< Block size used on the volume
    bool        disk_space_full_ = false;     ///< Flag indicating disk space is full

    size_t      disk_quota_ = 0;              ///< Maximum amount of disk space to use for storage (0 = no limit other than disk space)
    size_t      disk_quota_in_use_ = 0;       ///< Current amount of disk space in use by restaged bundles
    size_t      disk_num_files_ = 0;          ///< Current number of files in use by restaged bundles
    bool        disk_quota_full_ = false;     ///< Flag indicating disk quota is full

    // including these also for now in case they prove useful for BARD reporting
    uint64_t    days_retention_ = 30;          ///< Number of days to keep bundles stored
    bool        expire_bundles_ = false;      ///< Whether to delete bundles when they expire
    uint64_t    ttl_override_   = 0;          ///< Minimum number of seconds to set the time to live to when reloading
    uint64_t    auto_reload_interval_ = 0 ;   ///< How often (seconds) to try to auto reload bundles (0 = never)


    restage_cl_states_t cl_state_ = RESTAGE_CL_STATE_UNDEFINED;  ///< Current state of the Restage CL


};







class BARDRestageCLIF
{
public:
    BARDRestageCLIF() {}

    virtual ~BARDRestageCLIF(){}

   
   /**
    * Initiates an event to attempt to reload all types of restaged bundles
    * @param new_expiration Override the expiration time on reloaded bundles if needed to provided a minimum TTL
    * @return Number of directories queued for reloading
    */
    virtual int reload_all(size_t new_expiration) = 0;

   /**
    * Initiates an event to attempt to reload specified bndles
    * @param quota_type   The quota type of the restaged bundles to be reloaded (by source EID or destination EID)
    * @param scheme       The naming scheme of the restaged bundles to be reloaded
    * @param nodename     The node name/number of the restaged bundles to be reloaded
    * @param new_expiration Override the expiration time on reloaded bundles if needed to provided a minimum TTL
    * @param new_dest_eid Override the bundle destination EID on reloaed bundles
    * @return Number of directories queued for reloading
    */
    virtual int reload(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, std::string& nodename, 
                       size_t new_expiration, std::string& new_dest_eid) = 0;


   /**
    * Initiates an event to delete the specified bundles
    * @param quota_type   The quota type of the restaged bundles to be reloaded (by source EID or destination EID)
    * @param scheme       The naming scheme of the restaged bundles to be reloaded
    * @param nodename     The node name/number of the restaged bundles to be reloaded
    * @return Number of directories queued for deleting
    */
    virtual int delete_restaged_bundles(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme, 
                                        std::string& nodename) = 0;

   /**
    * Initiates events to delete all restaged bundles
    * @return Number of directories queued for deleting
    */
    virtual int delete_all_restaged_bundles() = 0;

   /**
    * Pause restaging and reloading in preparation to do a rescan.
    */
   virtual void pause_for_rescan() = 0;


   /**
    * Pause restaging and reloading in preparation to do a rescan.
    */
   virtual void resume_after_rescan() = 0;

   /**
    * Initiate a rescan of the external storage
    */
   virtual void rescan() = 0;

   virtual void send_email_notifications(std::string& message, std::string& message2) = 0;

public:






};

}    

#endif // BARD_ENABLED

#endif 
