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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BARD_ENABLED

#include "BARDRestageCLIF.h"

namespace dtn {


// Data structures shared between the Bundle Restaging Daemon and the Restage Convergence Layer


int bard_quota_type_to_int(bard_quota_type_t quota_type)
{
    return static_cast<int>(quota_type);
};

const char* bard_quota_type_to_str(bard_quota_type_t quota_type)
{
    switch (quota_type) {
        case(BARD_QUOTA_TYPE_DST): return "dst";
        case(BARD_QUOTA_TYPE_SRC): return "src";
        default:
            return "unk";
    }
}

bard_quota_type_t int_to_bard_quota_type(int quota_type)
{
    if (quota_type == static_cast<int>(BARD_QUOTA_TYPE_DST)) {
        return BARD_QUOTA_TYPE_DST;
    } else if (quota_type == static_cast<int>(BARD_QUOTA_TYPE_SRC)) {
        return BARD_QUOTA_TYPE_SRC;
   } else {
       return BARD_QUOTA_TYPE_UNDEFINED;
   }
};

bard_quota_type_t str_to_bard_quota_type(const char* quota_type)
{
    if (0 == strcasecmp(quota_type, "dst")) {
        return BARD_QUOTA_TYPE_DST;
    } else if (0 == strcasecmp(quota_type, "src")) {
        return BARD_QUOTA_TYPE_SRC;
   } else {
       return BARD_QUOTA_TYPE_UNDEFINED;
   }
};


int bard_naming_scheme_to_int(bard_quota_naming_schemes_t scheme)
{
    return static_cast<int>(scheme);
};

const char* bard_naming_scheme_to_str(bard_quota_naming_schemes_t scheme)
{
    switch (scheme) {
        case(BARD_QUOTA_NAMING_SCHEME_IPN): return "ipn";
        case(BARD_QUOTA_NAMING_SCHEME_DTN): return "dtn";
        case(BARD_QUOTA_NAMING_SCHEME_IMC): return "imc";
        default:
            return "unk";
    }
}

bard_quota_naming_schemes_t int_to_bard_naming_scheme(int scheme)
{
    if (scheme == static_cast<int>(BARD_QUOTA_NAMING_SCHEME_IPN)) {
        return BARD_QUOTA_NAMING_SCHEME_IPN;
    } else  if (scheme == static_cast<int>(BARD_QUOTA_NAMING_SCHEME_DTN)) {
        return BARD_QUOTA_NAMING_SCHEME_DTN;
    } else  if (scheme == static_cast<int>(BARD_QUOTA_NAMING_SCHEME_IMC)) {
        return BARD_QUOTA_NAMING_SCHEME_IMC;
   } else {
       return BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
   }
};

bard_quota_naming_schemes_t str_to_bard_naming_scheme(const char* scheme)
{
    if (0 == strcasecmp(scheme, "ipn")) {
        return BARD_QUOTA_NAMING_SCHEME_IPN;
    } else if (0 == strcasecmp(scheme, "dtn")) {
        return BARD_QUOTA_NAMING_SCHEME_DTN;
    } else if (0 == strcasecmp(scheme, "imc")) {
        return BARD_QUOTA_NAMING_SCHEME_IMC;
   } else {
        return BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
   }
};


const char* restage_cl_states_to_str(restage_cl_states_t state)
{
    switch (state) {
        case(RESTAGE_CL_STATE_ONLINE): return "ONLINE";
        case(RESTAGE_CL_STATE_LOW): return "LOW_QUOTA";
        case(RESTAGE_CL_STATE_HIGH): return "HIGH_QUOTA";
        case(RESTAGE_CL_STATE_FULL_QUOTA): return "FULL_QUOTA";
        case(RESTAGE_CL_STATE_FULL_DISK): return "FULL_DISK";
        case(RESTAGE_CL_STATE_ERROR): return "ERROR";
        case(RESTAGE_CL_STATE_SHUTDOWN): return "SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

restage_cl_states_t str_to_restage_cl_state(const char* state)
{
    if (0 == strcasecmp(state, "ONLINE")) {
        return RESTAGE_CL_STATE_ONLINE;
    } else if (0 == strcasecmp(state, "LOW_QUOTA")) {
        return RESTAGE_CL_STATE_LOW;
    } else if (0 == strcasecmp(state, "HIGH_QUOTA")) {
        return RESTAGE_CL_STATE_HIGH;
    } else if (0 == strcasecmp(state, "FULL_QUOTA")) {
        return RESTAGE_CL_STATE_FULL_QUOTA;
    } else if (0 == strcasecmp(state, "FULL_DISK")) {
        return RESTAGE_CL_STATE_FULL_DISK;
    } else if (0 == strcasecmp(state, "ERROR")) {
        return RESTAGE_CL_STATE_ERROR;
    } else if (0 == strcasecmp(state, "SHUTDOWN")) {
        return RESTAGE_CL_STATE_SHUTDOWN;
   } else {
        return RESTAGE_CL_STATE_UNDEFINED;;
   }
};

oasys::SpinLock* 
RestageCLStatus::lock()
{
    return &lock_;
}


std::string
RestageCLStatus::restage_link_name()
{
    std::string result;

    oasys::ScopeLock scoplock(&lock_, __func__);

    result = restage_link_name_;

    return result;
}


std::string
RestageCLStatus::storage_path()
{
    std::string result;

    oasys::ScopeLock scoplock(&lock_, __func__);

    result = storage_path_;

    return result;
}


std::string
RestageCLStatus::validated_mount_pt()
{
    std::string result;

    oasys::ScopeLock scoplock(&lock_, __func__);

    result = validated_mount_pt_;

    return result;
}


bool
RestageCLStatus::mount_point()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return mount_point_;
}

bool
RestageCLStatus::mount_pt_validated()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return mount_pt_validated_;
}

bool
RestageCLStatus::storage_path_exists()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return storage_path_exists_;
}


bool
RestageCLStatus::part_of_pool()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return part_of_pool_;
}

bool
RestageCLStatus::email_enabled()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return email_enabled_;
}


size_t
RestageCLStatus::vol_total_space()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return vol_total_space_;
}

size_t
RestageCLStatus::vol_space_available()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return vol_space_available_;
}

bool
RestageCLStatus::disk_space_full()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return disk_space_full_;
}

size_t
RestageCLStatus::vol_block_size()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return vol_block_size_;
}

size_t
RestageCLStatus::disk_quota()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return disk_quota_;
}

size_t
RestageCLStatus::disk_quota_in_use()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return disk_quota_in_use_;
}

size_t
RestageCLStatus::disk_num_files()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return disk_num_files_;
}

bool
RestageCLStatus::disk_quota_full()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return disk_quota_full_;
}


    // including these also for now in case they prove useful for BARD reporting
uint64_t
RestageCLStatus::days_retention()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return days_retention_;
}

bool
RestageCLStatus::expire_bundles()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return expire_bundles_;
}

uint64_t
RestageCLStatus::ttl_override()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return ttl_override_;
}

uint64_t
RestageCLStatus::auto_reload_interval()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return auto_reload_interval_;
}



restage_cl_states_t
RestageCLStatus::cl_state()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return cl_state_;
}


const char* 
RestageCLStatus::cl_state_str()
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    return restage_cl_states_to_str(cl_state_);
}

void
RestageCLStatus::set_restage_link_name(const std::string& restage_link_name)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    restage_link_name_ = restage_link_name;
}


void
RestageCLStatus::set_storage_path(const std::string& storage_path)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    storage_path_ = storage_path;
}

void
RestageCLStatus::set_validated_mount_pt(const std::string& validated_mount_pt)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    validated_mount_pt_ = validated_mount_pt;
}


void
RestageCLStatus::set_mount_point(bool mount_point)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    mount_point_ = mount_point;
}

void
RestageCLStatus::set_mount_pt_validated(bool mount_pt_validated)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    mount_pt_validated_ = mount_pt_validated;
}

void
RestageCLStatus::set_storage_path_exists(bool storage_path_exists)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    storage_path_exists_ = storage_path_exists;
}


void
RestageCLStatus::set_part_of_pool(bool part_of_pool)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    part_of_pool_ = part_of_pool;
}

void
RestageCLStatus::set_email_enabled(bool email_enabled)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    email_enabled_ = email_enabled;
}


void
RestageCLStatus::set_vol_total_space(size_t vol_total_space)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    vol_total_space_ = vol_total_space;
}

void
RestageCLStatus::set_vol_space_available(size_t vol_space_available)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    vol_space_available_ = vol_space_available;
}

void
RestageCLStatus::inc_vol_space_available(size_t bytes)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    vol_space_available_ += bytes;
}

void
RestageCLStatus::dec_vol_space_available(size_t bytes)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    if (vol_space_available_ >= bytes) {
        vol_space_available_ -= bytes;
    } else {
        vol_space_available_ = 0;
    }
}

void
RestageCLStatus::set_disk_space_full(bool disk_space_full)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    disk_space_full_ = disk_space_full;
}

void
RestageCLStatus::set_vol_block_size(size_t block_size)
{
    oasys::ScopeLock scoplock(&lock_, __func__);

    if (block_size == 0) {
        vol_block_size_ = 4096;
    } else {
        vol_block_size_ = block_size;
    }
}


void
RestageCLStatus::set_disk_quota(size_t disk_quota)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    disk_quota_ = disk_quota;
}

void
RestageCLStatus::set_disk_quota_in_use(size_t disk_quota_in_use)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    disk_quota_in_use_ = disk_quota_in_use;
}

void
RestageCLStatus::set_disk_num_files(size_t disk_num_files)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    disk_num_files_ = disk_num_files;
}

void
RestageCLStatus::set_disk_quota_full(bool disk_quota_full)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    disk_quota_full_ = disk_quota_full;
}

void
RestageCLStatus::set_days_retention(uint64_t days_retention)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    days_retention_ = days_retention;
}

void
RestageCLStatus::set_expire_bundles(bool expire_bundles)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    expire_bundles_ = expire_bundles;
}

void
RestageCLStatus::set_ttl_override(uint64_t ttl_override)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    ttl_override_ = ttl_override;
}

void
RestageCLStatus::set_auto_reload_interval(uint64_t auto_reload_interval)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    auto_reload_interval_ = auto_reload_interval;
}


void
RestageCLStatus::set_cl_state(restage_cl_states_t cl_state)
{
    oasys::ScopeLock scoplock(&lock_, __func__);
    cl_state_ = cl_state;
}


}    

#endif // BARD_ENABLED

