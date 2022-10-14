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


#ifdef BARD_ENABLED


#include "BARDNodeStorageUsage.h"


namespace dtn {


//----------------------------------------------------------------------
std::string
BARDNodeStorageUsage::make_key(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, uint64_t node_number)
{
    std::string result;

    char buf[256];
    const char* src_dst = "dst";
    const char* scheme = "ipn";

    if (quota_type == BARD_QUOTA_TYPE_SRC) {
        src_dst = "src";
    }

    switch (naming_scheme) {
        case BARD_QUOTA_NAMING_SCHEME_IPN:
            //scheme = "ipn";
            break;

        case BARD_QUOTA_NAMING_SCHEME_DTN:
            scheme = "dtn";
            break;

        case BARD_QUOTA_NAMING_SCHEME_IMC:
            scheme = "imc";
            break;

        default:
            scheme = "unk";
            log_always_p("/dzddebug", "ERROR - make_key %d unknown scheme", __LINE__);
    }

    snprintf(buf, sizeof(buf), "%s_%s_%20" PRIu64, src_dst, scheme, node_number);

    result = buf;

    return result;
}

//----------------------------------------------------------------------
std::string
BARDNodeStorageUsage::make_key(bard_quota_type_t quota_type, bard_quota_naming_schemes_t naming_scheme, std::string& nodename)
{
    std::string result;

    char buf[256];
    const char* src_dst = "dst";
    const char* scheme = "ipn";
    bool name_is_numeric = true;

    if (quota_type == BARD_QUOTA_TYPE_SRC) {
        src_dst = "src";
    }

    switch (naming_scheme) {
        case BARD_QUOTA_NAMING_SCHEME_IPN:
            //scheme = "ipn";
            break;

        case BARD_QUOTA_NAMING_SCHEME_DTN:
            scheme = "dtn";
            name_is_numeric = false;
            break;

        case BARD_QUOTA_NAMING_SCHEME_IMC:
            scheme = "imc";
            break;

        default:
            scheme = "unk";
            log_always_p("/dzdebug", "ERROR - make_key %d unknown scheme", __LINE__);
    }

    if (name_is_numeric) {
        snprintf(buf, sizeof(buf), "%s_%s_%20s", src_dst, scheme, nodename.c_str());
    } else {
        snprintf(buf, sizeof(buf), "%s_%s_%-20s", src_dst, scheme, nodename.c_str());
    }

    result = buf;

    return result;
}








//----------------------------------------------------------------------
BARDNodeStorageUsage::BARDNodeStorageUsage()
{
}


//----------------------------------------------------------------------
BARDNodeStorageUsage::BARDNodeStorageUsage(bard_quota_type_t quota_type, 
                                         bard_quota_naming_schemes_t naming_scheme,
                                         uint64_t node_number)
{

    set_quota_type(quota_type);
    set_naming_scheme(naming_scheme);
    set_node_number(node_number);
}

//----------------------------------------------------------------------
BARDNodeStorageUsage::BARDNodeStorageUsage(bard_quota_type_t quota_type, 
                                         bard_quota_naming_schemes_t naming_scheme,
                                         std::string& nodename)
{
    set_quota_type(quota_type);
    set_naming_scheme(naming_scheme);
    set_nodename(nodename);
}

//----------------------------------------------------------------------
BARDNodeStorageUsage::BARDNodeStorageUsage(const oasys::Builder&)
{}

//----------------------------------------------------------------------
BARDNodeStorageUsage::~BARDNodeStorageUsage ()
{}


//----------------------------------------------------------------------
bool
BARDNodeStorageUsage::operator< (const BARDNodeStorageUsage& other)
{
    bool result = false;

    if (other.quota_type_ < quota_type_) {
        result = true;
    } else if (other.naming_scheme_ < naming_scheme_) {
        result = true;
    } else {
        // down to comparing actual node number/name
        if (naming_scheme_ == BARD_QUOTA_NAMING_SCHEME_DTN) {
            result = (-1 == other.nodename_.compare(nodename_));
        } else {
            result = (other.node_number_ < node_number_);
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::serialize(oasys::SerializeAction* a)
{
    durable_key();  // assure the key string is initialized properly before stoorage

    a->process("key", &key_);

    int tmp_quota_type = bard_quota_type_to_int(quota_type_);
    int tmp_naming_scheme = bard_naming_scheme_to_int(naming_scheme_);

    a->process("quota_type", &tmp_quota_type);
    a->process("naming_scheme", &tmp_naming_scheme);
    a->process("node_number", &node_number_);
    a->process("nodename", &nodename_);

    a->process("internal_bundles", &quota_internal_bundles_);
    a->process("internal_bytes", &quota_internal_bytes_);
    a->process("external_bundles", &quota_external_bundles_);
    a->process("external_bytes", &quota_external_bytes_);
    a->process("refuse_bundle", &quota_refuse_bundle_);
    a->process("auto_reload", &quota_auto_reload_);
    a->process("restage_link_name", &quota_restage_link_name_);


    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        quota_in_datastore_ = true;

         set_quota_type(int_to_bard_quota_type(tmp_quota_type));
         set_naming_scheme(int_to_bard_naming_scheme(tmp_naming_scheme));

        key_initialized_ = false;
        durable_key();  // assure the key string is initialized after reading in from storage
    }
}

//----------------------------------------------------------------------
std::string
BARDNodeStorageUsage::durable_key()
{
    if (!key_initialized_) {

        if (naming_scheme_ == BARD_QUOTA_NAMING_SCHEME_DTN) {
            key_ = make_key(quota_type_, naming_scheme_, nodename_);
        } else {
            key_ = make_key(quota_type_, naming_scheme_, node_number_);
        }

        key_initialized_ = true;
    }

    return key_;
}


//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_type(bard_quota_type_t t)
{
    key_initialized_ = false;
    quota_modified_ = true;
    quota_type_ = t;
}

//----------------------------------------------------------------------
const char*
BARDNodeStorageUsage::quota_type_cstr()
{
    switch (quota_type_) {
        case BARD_QUOTA_TYPE_DST: return "dst";
        case BARD_QUOTA_TYPE_SRC: return "src";
        default:
            return "unk";
    }
}


//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_naming_scheme(bard_quota_naming_schemes_t t)
{
    key_initialized_ = false;
    quota_modified_ = true;
    naming_scheme_ = t;
}

//----------------------------------------------------------------------
const char*
BARDNodeStorageUsage::naming_scheme_cstr()
{
    switch (naming_scheme_) {
        case BARD_QUOTA_NAMING_SCHEME_IPN: return "ipn";
        case BARD_QUOTA_NAMING_SCHEME_DTN: return "dtn";
        case BARD_QUOTA_NAMING_SCHEME_IMC: return "imc";
        default:
            return "unk";
    }
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_node_number(uint64_t t)
{
    key_initialized_ = false;
    quota_modified_ = true;
    node_number_ = t;
    nodename_ = std::to_string(t);
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_nodename(std::string& t)
{
    key_initialized_ = false;
    quota_modified_ = true;
    nodename_ = t;
    node_number_ = strtoull(t.c_str(), nullptr, 10);
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_internal_bundles(uint64_t t)
{
    quota_modified_ = true;
    quota_internal_bundles_ = t; 
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_internal_bytes(uint64_t t)
{
    quota_modified_ = true;
    quota_internal_bytes_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_external_bundles(uint64_t t)
{
    quota_modified_ = true;
    quota_external_bundles_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_external_bytes(uint64_t t)
{
    quota_modified_ = true;
    quota_external_bytes_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_refuse_bundle(bool t)
{
    quota_modified_ = true;
    quota_refuse_bundle_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_auto_reload(bool t)
{
    quota_modified_ = true;
    quota_auto_reload_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::set_quota_restage_link_name(std::string& t)
{
    quota_modified_ = true;
    quota_restage_link_name_ = t;
}

//----------------------------------------------------------------------
void
BARDNodeStorageUsage::clear_quota_restage_link_name()
{
    quota_modified_ = true;
    quota_restage_link_name_.clear();
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::committed_internal_bundles()
{
    return inuse_internal_bundles_ + reserved_internal_bundles_;
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::committed_internal_bytes()
{
    return inuse_internal_bytes_ + reserved_internal_bytes_;
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::committed_external_bundles()
{
    return inuse_external_bundles_ + reserved_external_bundles_;
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::committed_external_bytes()
{
    return inuse_external_bytes_ + reserved_external_bytes_;
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::last_committed_external_bundles()
{
    return last_inuse_external_bundles_ + reserved_external_bundles_;
}

//----------------------------------------------------------------------
size_t
BARDNodeStorageUsage::last_committed_external_bytes()
{
    return last_inuse_external_bytes_ + reserved_external_bytes_;
}

//----------------------------------------------------------------------
bool
BARDNodeStorageUsage::over_quota(size_t& bundles_over, size_t& bytes_over)
{
    bool result = false;
    bundles_over = 0;
    bytes_over = 0;

    if (quota_internal_bundles_ > 0) {
        if (inuse_internal_bundles_ > quota_internal_bundles_) {
            result = true;
            bundles_over = inuse_internal_bundles_ - quota_internal_bundles_;
        }
    }

    if (quota_internal_bytes_ > 0) {
        if (inuse_internal_bytes_ > quota_internal_bytes_) {
            result = true;
            bytes_over = inuse_internal_bytes_ - quota_internal_bytes_;
        }
    }

    return result;
}


//----------------------------------------------------------------------
void
BARDNodeStorageUsage::copy_data(BARDNodeStorageUsage* other)
{
    // elements for the key
    quota_type_ = other->quota_type_;
    naming_scheme_ = other->naming_scheme_;
    node_number_ = other->node_number_;
    nodename_ = other->nodename_;

    key_initialized_ = other->key_initialized_;
    key_ = other->key_;

    // quota elements
    quota_internal_bundles_ = other->quota_internal_bundles_;
    quota_internal_bytes_ = other->quota_internal_bytes_;
    quota_external_bundles_ = other->quota_external_bundles_;
    quota_external_bytes_ = other->quota_external_bytes_;
    quota_refuse_bundle_ = other->quota_refuse_bundle_;
    quota_auto_reload_ = other->quota_auto_reload_;
    quota_restage_link_name_ = other->quota_restage_link_name_;

    // usage elements
    inuse_internal_bundles_ = other->inuse_internal_bundles_;
    inuse_internal_bytes_ = other->inuse_internal_bytes_;
    inuse_external_bundles_ = other->inuse_external_bundles_;
    inuse_external_bytes_ = other->inuse_external_bytes_;

    reserved_internal_bundles_ = other->reserved_internal_bundles_;
    reserved_internal_bytes_ = other->reserved_internal_bytes_;
    reserved_external_bundles_ = other->reserved_external_bundles_;
    reserved_external_bytes_ = other->reserved_external_bytes_;
}    


} // namespace dtn


#endif  // BARD_ENABLED
