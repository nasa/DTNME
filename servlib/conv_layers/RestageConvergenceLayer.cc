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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#if BARD_ENABLED


#include <dirent.h>
#include <memory>
#include <mntent.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <unistd.h>


#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/StringBuffer.h>

#include "RestageConvergenceLayer.h"

#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleArchitecturalRestagingDaemon.h"
#include "bundling/FormatUtils.h"
#include "naming/IPNScheme.h"
#include "naming/DTNScheme.h"
#include "naming/IMCScheme.h"

namespace dtn {


// Note that the separator characters are configurable in case 
// someone has an issue with some DTN Scheme names down the road

char FIELD_SEPARATOR_CHAR = '_';       // underscore
char EID_FIELD_SEPARATOR_CHAR = '-';   // dash

std::string FIELD_SEPARATOR_STR = "_";       // underscore
std::string EID_FIELD_SEPARATOR_STR = "-";   // dash

// major tokens in the directory and file names
// names will always start with one of these two
std::string TOKEN_SRC = "src_";
std::string TOKEN_DST = "dst_";

#define TOKEN_SRC_LEN  4
#define TOKEN_DST_LEN  4

// embedded tokens are used within a name 
// and have a field separator before and after them to simplify formatting and parsing
std::string TOKEN_DST_EMBEDDED = "_dst_";
std::string TOKEN_BTS_EMBEDDED = "_bts_";
std::string TOKEN_FRG_EMBEDDED = "_frg_";
std::string TOKEN_PAY_EMBEDDED = "_pay_";
std::string TOKEN_EXP_EMBEDDED = "_exp_";

#define TOKEN_DST_EMBEDDED_LEN   5
#define TOKEN_BTS_EMBEDDED_LEN   5
#define TOKEN_FRG_EMBEDDED_LEN   5
#define TOKEN_PAY_EMBEDDED_LEN   5
#define TOKEN_EXP_EMBEDDED_LEN   5

// EID representation in a file name always start with one of these three tokens
std::string TOKEN_IPN = "ipn-";
std::string TOKEN_DTN = "dtn-";
std::string TOKEN_IMC = "imc-";

#define TOKEN_IPN_LEN  4
#define TOKEN_DTN_LEN  4
#define TOKEN_IMC_LEN  4

#define HUMAN_TIMESTAMP_LEN  15

#define FILEMODE_ALL (S_IRWXU | S_IRWXG | S_IRWXO )

class RestageConvergenceLayer::Params RestageConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
RestageConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("storage_path", &storage_path_);
    a->process("mount_point", &mount_point_);
    a->process("days_retention", &days_retention_);
    a->process("expire_bundles", &expire_bundles_);
    a->process("ttl_override", &ttl_override_);
    a->process("auto_reload_interval", &auto_reload_interval_);
    a->process("disk_quota", &disk_quota_);
    a->process("part_of_pool", &part_of_pool_);
    a->process("eamil_enabled", &email_enabled_);
    a->process("min_disk_space", &min_disk_space_available_);
    a->process("min_quota_avail", &min_quota_available_);
}

//----------------------------------------------------------------------
RestageConvergenceLayer::RestageConvergenceLayer()
    : ConvergenceLayer("RestageConvergenceLayer", "restage")
{
}

//----------------------------------------------------------------------
CLInfo*
RestageConvergenceLayer::new_link_params()
{
    return new RestageConvergenceLayer::Params(RestageConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
SPtr_CLInfo
RestageConvergenceLayer::new_link_params_sptr()
{
    SPtr_CLInfo sptr_clinfo = std::make_shared<CLInfo>(Params(RestageConvergenceLayer::defaults_));
    return sptr_clinfo;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&RestageConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    params->field_separator_set_ = false;
    params->eid_field_separator_set_ = false;

    oasys::OptParser p;

    p.addopt(new oasys::BoolOpt("mount_point", &params->mount_point_));
    p.addopt(new oasys::UInt64Opt("days_retention", &params->days_retention_));
    p.addopt(new oasys::BoolOpt("expire_bundles", &params->expire_bundles_));
    p.addopt(new oasys::UInt64Opt("ttl_override", &params->ttl_override_));
    p.addopt(new oasys::UInt64Opt("auto_reload_interval", &params->auto_reload_interval_));
    p.addopt(new oasys::SizeOpt("disk_quota", &params->disk_quota_));
    p.addopt(new oasys::BoolOpt("part_of_pool", &params->part_of_pool_));
    p.addopt(new oasys::BoolOpt("email_enabled", &params->email_enabled_));
    p.addopt(new oasys::SizeOpt("min_disk_space", &params->min_disk_space_available_));
    p.addopt(new oasys::SizeOpt("min_quota_avail", &params->min_quota_available_));
    p.addopt(new oasys::StringOpt("field_separator", &params->field_separator_, "", "", &params->field_separator_set_));
    p.addopt(new oasys::StringOpt("eid_field_separator", &params->eid_field_separator_, "", "", &params->eid_field_separator_set_));


    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        log_err("error parsing link options: invalid option '%s'", *invalidp);
        return false;
    } else {
        argc -= count;
        
        parse_params_for_emails(params, argc, argv);

        oasys::ScopeLock scoplok(&params->sptr_emails_->lock_, __func__);

        if (params->sptr_emails_->email_list_.size() > 0) {
            if (params->sptr_emails_->from_email_.empty()) {
                log_err("No from_email specified for alerts - disabling email");
                params->email_enabled_ = false;
            }
        }

    }

    if (params->field_separator_set_) {
        if (params->field_separator_.length() != 1) {
            log_err("Invalid field separator specified - must be 1 character long: %s", params->field_separator_.c_str());
            return false;
        }
    }

    if (params->eid_field_separator_set_) {
        if (params->eid_field_separator_.length() != 1) {
            log_err("Invalid EID field separator specified - must be 1 character long: %s", params->eid_field_separator_.c_str());
            return false;
        }
    }

    if (0 == params->eid_field_separator_.compare(params->field_separator_)) {
        log_err("EID Field Separator must be different from the Field Separator");
        return false;
    }

    return true;
};

//----------------------------------------------------------------------
void
RestageConvergenceLayer::parse_params_for_emails(Params* params, int argc, const char** argv)
{
    // the params are good if it made it here
    // initialze the EmailNotifications object
    if (!params->sptr_emails_) {
        params->sptr_emails_ = std::make_shared<EmailNotifications>();
    }

    for (int ix = 0; ix < argc; ++ix) {
        if (0 == strncmp(argv[ix], "from_email=", 11)) {
            const char* email_cstr = &argv[ix][11];
            std::string email_addr = email_cstr;

            // minimal checking for a valid email address format
            auto at = std::find(email_addr.begin(), email_addr.end(), '@');
            auto dot = std::find(at, email_addr.end(), '.');

            if ((at == email_addr.end()) || (dot == email_addr.end())) {
                log_err("error parsing link options: invalid from_email address: %s", argv[ix]);
            } else {
                oasys::ScopeLock scoplok(&params->sptr_emails_->lock_, __func__);

                params->sptr_emails_->from_email_ = email_addr;
            }

        } else if (0 == strncmp(argv[ix], "add_email=", 10)) {
            const char* email_cstr = &argv[ix][10];
            std::string email_addr = email_cstr;

            // minimal checking for a valid email address format
            auto at = std::find(email_addr.begin(), email_addr.end(), '@');
            auto dot = std::find(at, email_addr.end(), '.');

            if ((at == email_addr.end()) || (dot == email_addr.end())) {
                log_err("error parsing link options: invalid email address: %s", argv[ix]);
            } else {
                oasys::ScopeLock scoplok(&params->sptr_emails_->lock_, __func__);

                params->sptr_emails_->email_list_[email_addr] = true;
            }

        } else if (0 == strncmp(argv[ix], "del_email=", 10)) {
            const char* email_cstr = &argv[ix][10];
            std::string email_addr = email_cstr;

            EmailListIter iter = params->sptr_emails_->email_list_.find(email_addr);
            if (iter != params->sptr_emails_->email_list_.end()) {
                params->sptr_emails_->email_list_.erase(email_addr);
            }
        } else {
            log_err("error parsing link options: invalid option: %s", argv[ix]);
        }
    }

}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[]) 
{
    (void) link;

    bool result = true;

    const char* invalid;

//dzdebug    Params* params = (Params*)link->cl_info();
    SPtr_RestageCLParams sptr_params = std::dynamic_pointer_cast<Params>(link->sptr_cl_info());

    if (! parse_params(sptr_params.get(), argc, argv, &invalid)) {
        log_err("%s: error parsing link options: invalid option '%s'", __func__, invalid);
        result = false;
    }

    const ContactRef contact = link->contact();
    SPtr_ExternalStorageController sptr_esc;
    sptr_esc = std::dynamic_pointer_cast<ExternalStorageController>(contact->sptr_cl_info());

    sptr_esc->reconfigure(sptr_params);

    if (sptr_params->field_separator_set_) {
        log_err("The Field Separator cannot be reconfigured on the fly - other changes will be applied");
        return false;
    }

    if (sptr_params->eid_field_separator_set_) {
        log_err("The EID Field Separator cannot be reconfigured on the fly - other changes will be applied");
        return false;
    }

    return result;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::reconfigure_link(const LinkRef& link,
                                         AttributeVector& av_params)
{
//dzdebug    Params* params = (Params*)link->cl_info();
    SPtr_RestageCLParams sptr_params = std::dynamic_pointer_cast<Params>(link->sptr_cl_info());

    AttributeVector::iterator iter;

    for (iter = av_params.begin(); iter != av_params.end(); iter++) {
        if (iter->name().compare("mount_point") == 0) { 
            sptr_params->mount_point_ = iter->bool_val();

        } else if (iter->name().compare("expire_bundles") == 0) { 
            sptr_params->expire_bundles_ = iter->bool_val();

        } else if (iter->name().compare("part_of_pool") == 0) { 
            sptr_params->part_of_pool_ = iter->bool_val();

        } else if (iter->name().compare("email_enabled") == 0) { 
            sptr_params->email_enabled_ = iter->bool_val();

        } else if (iter->name().compare("days_retention") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->days_retention_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->days_retention_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->days_retention_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->days_retention_ = iter->int64_val();
            }

        } else if (iter->name().compare("ttl_override") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->ttl_override_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->ttl_override_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->ttl_override_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->ttl_override_ = iter->int64_val();
            }

        } else if (iter->name().compare("auto_reload_interval") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->auto_reload_interval_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->auto_reload_interval_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->auto_reload_interval_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->auto_reload_interval_ = iter->int64_val();
            }

        } else if (iter->name().compare("disk_quota") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->disk_quota_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->disk_quota_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->disk_quota_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->disk_quota_ = iter->int64_val();
            }

        } else if (iter->name().compare("min_disk_space") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->min_disk_space_available_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->min_disk_space_available_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->min_disk_space_available_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->min_disk_space_available_ = iter->int64_val();
            }

        } else if (iter->name().compare("min_quota_avail") == 0) {
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                sptr_params->min_quota_available_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                sptr_params->min_quota_available_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                sptr_params->min_quota_available_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                sptr_params->min_quota_available_ = iter->int64_val();
            }

        } else if (iter->name().compare("from_email") == 0) { 

            std::string email_addr = iter->string_val();

            // minimal checking for a valid email address format
            auto at = std::find(email_addr.begin(), email_addr.end(), '@');
            auto dot = std::find(at, email_addr.end(), '.');

            if ((at == email_addr.end()) || (dot == email_addr.end())) {
                log_err("%s: invalid from_email address: %s",
                        __func__, email_addr.c_str());
            } else {
                oasys::ScopeLock scoplok(&sptr_params->sptr_emails_->lock_, __func__);

                sptr_params->sptr_emails_->from_email_ = email_addr;
            }

        } else if (iter->name().compare("add_email") == 0) { 

            std::string email_addr = iter->string_val();

            // minimal checking for a valid email address format
            auto at = std::find(email_addr.begin(), email_addr.end(), '@');
            auto dot = std::find(at, email_addr.end(), '.');

            if ((at == email_addr.end()) || (dot == email_addr.end())) {
                log_err("%s: invalid email address: %s",
                        __func__, email_addr.c_str());
            } else {
                oasys::ScopeLock scoplok(&sptr_params->sptr_emails_->lock_, __func__);

                sptr_params->sptr_emails_->email_list_[email_addr] = true;
            }

        } else if (iter->name().compare("del_email") == 0) { 

            std::string email_addr = iter->string_val();

            EmailListIter iter = sptr_params->sptr_emails_->email_list_.find(email_addr);
            if (iter != sptr_params->sptr_emails_->email_list_.end()) {
                sptr_params->sptr_emails_->email_list_.erase(email_addr);
            }
        }
        // any others to change on the fly through the External Router interface?

        else {
            log_crit("reconfigure_link - unknown parameter: %s", iter->name().c_str());
        }
    }    

    const ContactRef contact = link->contact();
    SPtr_ExternalStorageController sptr_esc;
    sptr_esc = std::dynamic_pointer_cast<ExternalStorageController>(contact->sptr_cl_info());

    sptr_esc->reconfigure(sptr_params);

}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Restage Convergence Layer [%s] - valid Link options:\n\n", name());
    buf.append("<next hop> format for \"link add\" command is <path to storage location> \n\n");
    buf.append("CLA specific options:\n");

    buf.append("    mount_point <Bool>                 - Whether to verify <path to storage location> is on a mounted drive (default: true)\n");
    buf.append("    days_retention <U64>               - Maximum number of days to store bundles for retrieval (default: 7)\n");
    buf.append("    expire_bundles <Bool>              - Whether to delete bundles from storage when they expire (default: false)\n");
    buf.append("    ttl_override <U64>                 - Override bundle expiration if necessary to provide specified minimum \n"
               "                                            number of seconds time to live (TTL) on reload  (default: 0)\n");
    buf.append("    auto_reload_interval <U64>         - How often (seconds) to try to automatically reload bundles (0=never; default: 3600)\n");
    buf.append("    disk_quota <Size>                  - Max disk space to use for storage (default: 0 = no limit other than disk space)\n");
    buf.append("    min_disk_space <Size>              - Minimum disk space needed to declare state ONLINE vs FULL (default = 100MB)\n");
    buf.append("    min_quota_avail <Size>             - Minimum quota space needed to declare state ONLINE after a FULL state (default = 1MB)\n");
    buf.append("    part_of_pool <Bool>                - Whether this instance is part of the BARD pool of storage locations (default: true)\n");
    buf.append("    email_enabled <Bool>               - Whether to send alert emails (default: true)\n");
    buf.append("    from_email <String>                - Email address to use as the sender of alerts\n");
    buf.append("    add_email <String>                 - Add an email address for alerts; (may be specified multiple times)\n");
    buf.append("    del_email <String>                 - Remove an email address from alert notifications; (may be specified multiple times)\n");
    buf.appendf("\n");
    buf.append("          (parameters of type <U64> and <Size> can include magnitude character (K, M or G):   125G = 125,000,000,000)\n");

    buf.append("\nOptions for all links:\n");
    
    buf.append("    <not applicable to the Restage Convergence Layer>\n");

    buf.append("\n");
    buf.append("Example:\n");
    buf.append("link add restage1 /data01/ext_storage ALWAYSON restage mount_point=true ttl_override=86400 auto_reload_interval=3600\n");
    buf.append("    (create a link named \"restage1\" with top level storage location of /data01/ext_storage;\n");
    buf.append("     * verify that /data01/ext_storage or /data01 is mounted;\n");
    buf.append("         if /data01 is a mount point but /data01/ext_storage does not exist then it will be created; \n");
    buf.append("     * retain bundles for up to 7 days regardless of their expiration time;\n");
    buf.append("     * try to reload bundles to internal storage every hour and override the bundle TTL if needed to provide a minimum of 1 day;\n");
    buf.append("     * it is part of the BARD pool and if it fills up, bundles configured to restage here can be sent to other members of the pool and vice versa;\n");
    buf.append("     * email alerts are enabled but no email addresses were specified so none will be sent;\n");
    buf.append("\n");
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Restage Convergence Layer [%s] should only be used as a Link\n", name());
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    (void) iface;
    (void) argc;
    (void) argv;

    log_err("Restage Convergence Layer should only be used as a Link and not as an Interface");
    return false;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::interface_activate(Interface* iface)
{
    (void) iface;

    log_err("Restage Convergence Layer should only be used as a Link and not as an Interface (interface_activate called)");
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::interface_down(Interface* iface)
{
    (void) iface;

    log_err("Restage Convergence Layer should only be used as a Link and not as an Interface (interface_down called)");
    return true;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    (void) iface;

    buf->append("Restage Convergence Layer should only be used as a Link and not as an Interface");
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->sptr_cl_info() == nullptr);
    
    log_always("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
//dzdebug     Params* params = new Params(defaults_);
    SPtr_RestageCLParams sptr_params = std::make_shared<Params>(defaults_);

    const char* invalid;
    if (! parse_params(sptr_params.get(), argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
//dzdebug        delete params;
        return false;
    }

    sptr_params->storage_path_ = link->nexthop();

    SPtr_CLInfo sptr_clinfo = std::static_pointer_cast<CLInfo>(sptr_params);
    link->set_sptr_cl_info(sptr_clinfo);

    return true;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->sptr_cl_info() != nullptr);

    log_debug("RestageConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

//dzdebug    delete link->cl_info();
//dzdebug    link->set_cl_info(nullptr);
    link->release_sptr_cl_info();
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->sptr_cl_info() != nullptr);
        
    const ContactRef contact = link->contact();


    SPtr_ExternalStorageController sptr_esc;
    sptr_esc = std::dynamic_pointer_cast<ExternalStorageController>(contact->sptr_cl_info());

    if (!sptr_esc) {
        log_crit("dump_link called with contact *%p with no ExternalStorageController!!",
                 contact.object());
        buf->appendf("dump_link called with link *%p with contact *%p with no ExternalStorageController!!",
                     link.object(), contact.object());
        return;
    }

    sptr_esc->dump_link(buf);
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->sptr_cl_info() != nullptr);
    
    log_debug("RestageConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());
    
//dzdebug    Params* params = (Params*)link->cl_info();
    SPtr_RestageCLParams sptr_params = std::dynamic_pointer_cast<Params>(link->sptr_cl_info());

    
    // create a new ESC structure
    SPtr_ExternalStorageController sptr_esc = std::make_shared<ExternalStorageController>(link->contact(), link->name());
    SPtr_CLInfo sptr_clinfo = std::static_pointer_cast<CLInfo>(sptr_esc);

    sptr_esc->set_sptr(sptr_esc); 
    sptr_esc->init(sptr_params, link->name_str());
    sptr_esc->start();

    contact->set_sptr_cl_info(sptr_clinfo);
    BundleDaemon::post(new ContactUpEvent(link->contact()));
    
    return true;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::close_contact(const ContactRef& contact)
{
    SPtr_ExternalStorageController sptr_esc;
    sptr_esc = std::dynamic_pointer_cast<ExternalStorageController>(contact->sptr_cl_info());

    sptr_esc->shutdown();

    log_info("close_contact *%p", contact.object());

    contact->release_sptr_cl_info();
    
    return true;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) bundle;
    (void) link;
    return;
}

//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::ExternalStorageController(const ContactRef& contact, std::string link_name)
    : Logger("RestageCL::ExternalStorageController",
             "/dtn/cl/restage/%s", link_name.c_str()),
      Thread("RestageCL::ExternalStorageController"),
      contact_(contact.object(), "RestageCovergenceLayer::ExternalStorageController")
{
    sptr_clstatus_ = std::make_shared<RestageCLStatus>();
}

RestageConvergenceLayer::ExternalStorageController::~ExternalStorageController()
{
    if (sptr_clstatus_->cl_state_ != RESTAGE_CL_STATE_DELETED) {
        shutdown();
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::shutdown()
{
    set_should_stop();

    update_restage_cl_state(RESTAGE_CL_STATE_DELETED);

    // inform the BARD that this instance is exiting
    sptr_bard_->unregister_restage_cl(sptr_clstatus_->restage_link_name_);

    if (qptr_reloader_) {
        qptr_reloader_->shutdown();
    }

    if (qptr_restager_) {
        qptr_restager_->shutdown();
    }

    // release the self pointers to allow the object to be deleted
    sptr_clstatus_->sptr_restageclif_.reset();
    sptr_esc_self_.reset();

    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::set_sptr(SPtr_ExternalStorageController sptr_esc)
{
    sptr_esc_self_ = sptr_esc;
    //sptr_clinfo_self_ = std::static_pointer_cast<CLInfo>(sptr_esc);

    sptr_clstatus_->sptr_restageclif_ = std::dynamic_pointer_cast<BARDRestageCLIF>(sptr_esc);
}

//----------------------------------------------------------------------
bool
//dzdebug RestageConvergenceLayer::ExternalStorageController::init(Params* params, const std::string& link_name)
RestageConvergenceLayer::ExternalStorageController::init(SPtr_RestageCLParams sptr_params, const std::string& link_name)
{
    log_debug("initializing ExternalStorageController");

    sptr_params_ = sptr_params;

    initialize_separator_constants();

    oasys::ScopeLock scoplok(&sptr_clstatus_->lock_, __func__);

    sptr_clstatus_->restage_link_name_ = link_name;

    // copy paramter info to the status info for sharing with the BARD
    sptr_clstatus_->storage_path_ = sptr_params_->storage_path_;
    sptr_clstatus_->mount_point_ = sptr_params_->mount_point_;
    sptr_clstatus_->disk_quota_ = sptr_params_->disk_quota_;
    sptr_clstatus_->part_of_pool_ = sptr_params_->part_of_pool_;

    sptr_clstatus_->days_retention_ = sptr_params_->days_retention_;
    sptr_clstatus_->expire_bundles_ = sptr_params_->expire_bundles_;
    sptr_clstatus_->ttl_override_ = sptr_params_->ttl_override_;
    sptr_clstatus_->auto_reload_interval_ = sptr_params_->auto_reload_interval_;

    return true;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::initialize_separator_constants()
{
    FIELD_SEPARATOR_CHAR = sptr_params_->field_separator_[0];
    EID_FIELD_SEPARATOR_CHAR = sptr_params_->eid_field_separator_[0];

    FIELD_SEPARATOR_STR = sptr_params_->field_separator_;
    EID_FIELD_SEPARATOR_STR = sptr_params_->eid_field_separator_;

    // major tokens in the directory and file names
    // names will always start with one of these two
    TOKEN_SRC = "src" + sptr_params_->field_separator_;
    TOKEN_DST = "dst" + sptr_params_->field_separator_;


    // embedded tokens are used within a name 
    // and have a field separator before and after them to simplify formatting and parsing
    TOKEN_DST_EMBEDDED = sptr_params_->field_separator_ + "dst" + sptr_params_->field_separator_;
    TOKEN_BTS_EMBEDDED = sptr_params_->field_separator_ + "bts" + sptr_params_->field_separator_;
    TOKEN_FRG_EMBEDDED = sptr_params_->field_separator_ + "frg" + sptr_params_->field_separator_;
    TOKEN_PAY_EMBEDDED = sptr_params_->field_separator_ + "pay" + sptr_params_->field_separator_;
    TOKEN_EXP_EMBEDDED = sptr_params_->field_separator_ + "exp" + sptr_params_->field_separator_;

    TOKEN_IPN = "ipn" + sptr_params_->eid_field_separator_;
    TOKEN_DTN = "dtn" + sptr_params_->eid_field_separator_;
    TOKEN_IMC = "imc" + sptr_params_->eid_field_separator_;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::reconfigure(SPtr_RestageCLParams sptr_params)
{
    log_debug("reconfiguring ExternalStorageController");

    ASSERT(sptr_params_ == sptr_params);

    oasys::ScopeLock scoplok(&sptr_clstatus_->lock_, __func__);

    // copy parameter info to the status info for sharing with the BARD
    sptr_clstatus_->mount_point_ = sptr_params_->mount_point_;
    sptr_clstatus_->disk_quota_ = sptr_params_->disk_quota_;
    sptr_clstatus_->part_of_pool_ = sptr_params_->part_of_pool_;

    sptr_clstatus_->days_retention_ = sptr_params_->days_retention_;
    sptr_clstatus_->expire_bundles_ = sptr_params_->expire_bundles_;
    sptr_clstatus_->ttl_override_ = sptr_params_->ttl_override_;
    sptr_clstatus_->auto_reload_interval_ = sptr_params_->auto_reload_interval_;


    sptr_clstatus_->disk_num_files_ = grand_total_num_files_;
    sptr_clstatus_->disk_quota_in_use_ = grand_total_num_bytes_;

    if (sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_FULL) {
        if (sptr_params_->disk_quota_ > 0) {
            ssize_t space_available = sptr_params_->disk_quota_ - grand_total_num_bytes_;
            // Most bundles are les than 1 MB so using that as a threshold
            if (space_available >= 1000000) {
                update_restage_cl_state(RESTAGE_CL_STATE_ONLINE);
            }
        } else {
            // quota was removed
            update_restage_cl_state(RESTAGE_CL_STATE_ONLINE);
        }
    } else if (sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_ONLINE) {
        if (sptr_params_->disk_quota_ > 0) {
            ssize_t space_available = sptr_params_->disk_quota_ - grand_total_num_bytes_;
            // Most bundles are les than 1 MB so using that as a threshold
            if (space_available <= 0) {
                update_restage_cl_state(RESTAGE_CL_STATE_FULL);
            }
        }
    }
}
    
//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::dump_link(oasys::StringBuffer* buf)
{
    oasys::ScopeLock scoplok(&sptr_clstatus_->lock_, __func__);

    buf->appendf("storage_path: %s\n", sptr_clstatus_->storage_path_.c_str());
    if (sptr_clstatus_->mount_point_) {
        if (sptr_clstatus_->mount_pt_validated_) {
            buf->appendf("mount_point: true (validated as %s)\n", sptr_clstatus_->validated_mount_pt_.c_str());
        } else {
            buf->appendf("mount_point: true (not mounted)\n");
        }
    } else {
        buf->appendf("mount_point: false\n");
    }

    buf->appendf("field_separator: %s\n", sptr_params_->field_separator_.c_str());
    buf->appendf("eid_field_separator: %s\n", sptr_params_->eid_field_separator_.c_str());

    buf->appendf("days_retention: %" PRIu64 "\n", sptr_clstatus_->days_retention_);
    buf->appendf("expire_bundles: %s\n", sptr_clstatus_->expire_bundles_ ? "true" : "false");
    buf->appendf("ttl_override: %" PRIu64 "\n", sptr_clstatus_->ttl_override_);
    buf->appendf("auto_reload_interval: %" PRIu64 "\n", sptr_clstatus_->auto_reload_interval_);
    if (sptr_clstatus_->auto_reload_interval_ > 0) {
        const char *fmtstr="%Y-%m-%d %H:%M:%S";
        struct tm tmval_buf;
        struct tm *tmval;
        char date_str[64];

        time_t tmp_secs = auto_reload_timer_.sec_;

        // convert seconds to a time string
        tmval = gmtime_r(&tmp_secs, &tmval_buf);
        strftime(date_str, 64, fmtstr, tmval);

        size_t time_to_next = sptr_clstatus_->auto_reload_interval_ - 
                              (auto_reload_timer_.elapsed_ms() / 1000);
        buf->appendf("last reload attempt: %s GMT  (next in %zu seconds)\n", 
                     date_str, time_to_next);
    }

    buf->appendf("\n");
    buf->appendf("========== Statistics and Status ==========\n");
    buf->appendf("total restaged: %zu  (errors: %zu)\n", total_restaged_, total_restage_dupes_ignored_);
    buf->appendf("total reloaded: %zu\n", total_reloaded_);
    buf->appendf("total deleted: %zu\n", total_deleted_);

    buf->appendf("\nVolume Status:\n");
    buf->appendf("volume size: %s\n", FORMAT_WITH_MAG(sptr_clstatus_->vol_total_space_).c_str());
    buf->appendf("volume available: %s\n", FORMAT_WITH_MAG(sptr_clstatus_->vol_space_available_).c_str());
    buf->appendf("disk quota: %s\n", FORMAT_WITH_MAG(sptr_clstatus_->disk_quota_).c_str());
    buf->appendf("quota in use: %s\n", FORMAT_WITH_MAG(sptr_clstatus_->disk_quota_in_use_).c_str());

    scoplok.unlock();

    // output the email list if any
    oasys::ScopeLock email_scoplok(&sptr_params_->sptr_emails_->lock_, __func__);

    buf->appendf("\n");
    buf->appendf("email_enabled: %s\n", sptr_params_->email_enabled_ ? "true" : "false");
    buf->appendf("from_email: %s\n", sptr_params_->sptr_emails_->from_email_.c_str());
    buf->appendf("email list (%zu):\n", sptr_params_->sptr_emails_->email_list_.size());

    EmailListIter iter = sptr_params_->sptr_emails_->email_list_.begin();
    while (iter != sptr_params_->sptr_emails_->email_list_.end()) {
        buf->appendf("    %s\n", iter->first.c_str());
        ++iter;
    }

    email_scoplok.unlock();


    buf->appendf("\nFile list by directory:\n");
    dump_file_list(buf);

    buf->appendf("\nStatistics:\n");
    dump_statistics(buf);

}

//----------------------------------------------------------------------
const char*
RestageConvergenceLayer::ExternalStorageController::quota_type_to_cstr(bard_quota_type_t quota_type)
{
    switch (quota_type) {
        case BARD_QUOTA_TYPE_DST: return "dst";
        case BARD_QUOTA_TYPE_SRC: return "src";
        default:
            return "unk";
    }
}

//----------------------------------------------------------------------
const char*
RestageConvergenceLayer::ExternalStorageController::naming_scheme_to_cstr(bard_quota_naming_schemes_t scheme)
{
    switch (scheme) {
        case BARD_QUOTA_NAMING_SCHEME_IPN: return "ipn";
        case BARD_QUOTA_NAMING_SCHEME_DTN: return "dtn";
        case BARD_QUOTA_NAMING_SCHEME_IMC: return "imc";
        default:
            return "unk";
    }
}



//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::dump_statistics(oasys::StringBuffer* buf)
{
    size_t num_dirs = dir_stats_map_.size();

    if (num_dirs == 0) {
        buf->append("No bundles restaged to external storage\n");
    } else {
        SPtr_RestageDirStatistics sptr_dir_stats;

        // make a first pass through the list to deterine the longest node name
        uint32_t tmp_len;
        uint32_t name_len = 8;
        RestageDirStatisticsMapIter stats_iter = dir_stats_map_.begin();

        while (stats_iter != dir_stats_map_.end()) {
            sptr_dir_stats = stats_iter->second;
      
            tmp_len = sptr_dir_stats->nodename_.length();

            if (tmp_len > name_len) {
                name_len = tmp_len;
            }

            ++stats_iter;
        }

        // calc and allocate vars for aligning titles and dashes

        // layout with minmum lengths for node name and link name 
        //buf->appendf("Quota   Name     Node    |  External Storage\n");
        //buf->appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes\n");
        //buf->appendf("-----  ------  --------  |  --------:--------\n");

        // title 1
        uint32_t title1_pad_len_name = (name_len - 4) / 2;          // to center "Node" in the field
        uint32_t title1_name_len = name_len - title1_pad_len_name;  // remainder for left aligned "Node"

        uint32_t title2_pad_len_name = (name_len - 8) / 2;          // to center "Name/Num" in the field
        uint32_t title2_name_len = name_len - title2_pad_len_name;  // remainder for left aligned "Name/Num"


        char* dashes = (char*) calloc(1, name_len + 1);
        char* spaces = (char*) calloc(1, name_len + 1);

        memset(dashes, '-', name_len);
        memset(spaces, ' ', name_len);


        if (num_dirs == 1) {
            buf->append("Bundle Restaging Subdirectories (1 entry):\n\n");
        } else {
            buf->appendf("Bundle Restaging Subdirectories (%zu entries):\n\n", num_dirs);
        }

        // layout with minmum lengths for node name and link name 
        //buf->appendf("Quota   Name     Node    |  External Storage\n");
        //buf->appendf("Type   Scheme  Name/Num  |  Bundles :  Bytes\n");
        //buf->appendf("-----  ------  --------  |  --------:--------\n");

        buf->appendf("Quota   Name   %*.*s%-*s  |  External Storage\n",
                    title1_pad_len_name, title1_pad_len_name, spaces, title1_name_len, "Node");

        buf->appendf("Type   Scheme  %*.*s%-*s  |  Bundles :  Bytes\n",
                    title2_pad_len_name, title2_pad_len_name, spaces, title2_name_len, "Name/Num");


        buf->appendf("-----  ------  %*.*s  |  --------:--------\n", name_len, name_len, dashes);


        // now make a pass through the list outputing each rec
        stats_iter = dir_stats_map_.begin();

        while (stats_iter != dir_stats_map_.end()) {
            sptr_dir_stats = stats_iter->second;
     
            buf->appendf(" %3.3s     %3.3s   %*.*s  |    %5.5s :  %5.5s\n",
                        quota_type_to_cstr(sptr_dir_stats->quota_type_),
                        naming_scheme_to_cstr(sptr_dir_stats->scheme_),
                        name_len, name_len, sptr_dir_stats->nodename_.c_str(),
                        FORMAT_WITH_MAG(sptr_dir_stats->num_files_).c_str(),
                        FORMAT_WITH_MAG(sptr_dir_stats->total_size_).c_str());

            ++stats_iter;
        }
    }
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::dump_file_list(oasys::StringBuffer* buf)
{
    size_t num_dirs = restage_dir_map_.size();

    if (num_dirs == 0) {
        buf->append("No bundles restaged to external storage\n");
    } else {
        BundleFileDescMapIter file_iter;
        SPtr_BundleFileDesc sptr_bfd;


        SPtr_BundleFileDescMap sptr_bfd_map;
        RestageDirMapIter dir_iter = restage_dir_map_.begin();

        while (dir_iter != restage_dir_map_.end()) {
            sptr_bfd_map = dir_iter->second;

            // this is a new directory
            buf->appendf("%s - num files: %zu\n", dir_iter->first.c_str(), sptr_bfd_map->size());

            if (sptr_bfd_map->size() <= 5) {

                file_iter = sptr_bfd_map->begin();
    
                while (file_iter != sptr_bfd_map->end()) {
                    sptr_bfd = file_iter->second;

                    buf->appendf("    DiskUsage: %5.5s  File: %s\n",
                                 FORMAT_WITH_MAG(sptr_bfd->disk_usage_).c_str(),
                                 sptr_bfd->filename_.c_str());

                    ++file_iter;
                }

            } else {

                // print the first 3
                int ctr = 0;
                file_iter = sptr_bfd_map->begin();
    
                while ((file_iter != sptr_bfd_map->end()) && (ctr < 3)) {
                    sptr_bfd = file_iter->second;

                    buf->appendf("    DiskUsage: %5.5s  File: %s\n",
                                 FORMAT_WITH_MAG(sptr_bfd->disk_usage_).c_str(),
                                 sptr_bfd->filename_.c_str());

                    ++file_iter;
                    ++ctr;
                }

                buf->appendf("    ...\n");

                // print the last 2
                file_iter = sptr_bfd_map->end();
                --file_iter;
                --file_iter;
    
                while (file_iter != sptr_bfd_map->end()) {
                    sptr_bfd = file_iter->second;

                    buf->appendf("    DiskUsage: %5.5s  File: %s\n",
                                 FORMAT_WITH_MAG(sptr_bfd->disk_usage_).c_str(),
                                 sptr_bfd->filename_.c_str());

                    ++file_iter;
                }

            }

            ++dir_iter;
        }
    }
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::run()
{
    char threadname[16] = "ExtStoragCntrlr";
    pthread_setname_np(pthread_self(), threadname);


    oasys::Time init_timer;
    init_timer.get_time();

    sptr_bard_ = BundleDaemon::instance()->bundle_restaging_daemon();

    validate_external_storage();

    log_always("RestageCL initialization took %" PRIu64 " ms", init_timer.elapsed_ms());

    qptr_restager_ = std::unique_ptr<Restager>(new Restager(sptr_esc_self_, sptr_clstatus_->restage_link_name_,
                                                            contact_, sptr_params_));
    qptr_restager_->start();

    qptr_reloader_ = std::unique_ptr<Reloader>(new Reloader(sptr_clstatus_->restage_link_name_,
                                                            sptr_esc_self_, sptr_bard_));
    qptr_reloader_->start();


    oasys::Time error_state_timer;
    oasys::Time garbage_collection_timer;

    uint64_t five_minutes_in_ms = 5 * 60 * 1000;
    uint64_t one_hour_in_ms = 60 * 60 * 1000;

    error_state_timer.get_time();
    auto_reload_timer_.get_time();
    garbage_collection_timer.get_time();

    while (!should_stop()) {
        usleep(100000);

        if (sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_ERROR) {
            // every  5 minutes try to access the storage to see if the issue is resolved
            if (error_state_timer.elapsed_ms() >= five_minutes_in_ms) {
                validate_external_storage();
                error_state_timer.get_time();
            }
        }

        if (perform_rescan_) {
            rescan_external_storage();
        }

        if (sptr_params_->auto_reload_interval_ > 0) {
            uint64_t interval = sptr_params_->auto_reload_interval_ * 1000;

            if (auto_reload_timer_.elapsed_ms() >= interval) {
                size_t ttl_override = sptr_params_->ttl_override_;
                reload_all(ttl_override);
                auto_reload_timer_.get_time();
            }
        }

        if (garbage_collection_timer.elapsed_ms() >= one_hour_in_ms) {
            do_garbage_collection();
            garbage_collection_timer.get_time();
        }
    }
}
    
//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::rescan_external_storage()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    log_always("Initiating rescan of external storage");

    // clear/reset all in use statistics
    restage_dir_map_.clear();
    dir_stats_map_.clear();
    grand_total_num_files_ = 0;
    grand_total_num_bytes_ = 0;

    // update the status record that is shared with the BARD
    do {
        oasys::ScopeLock clstatus_scoplok(&sptr_clstatus_->lock_, __func__);

        sptr_clstatus_->disk_num_files_ = 0;
        sptr_clstatus_->disk_quota_in_use_ = 0;
    } while (false);  // limit scope of lock


    validate_external_storage();

    update_bard_usage_stats();

    log_always("Completed rescan of external storage");

    sptr_bard_->rescan_completed();

    perform_rescan_ = false;
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::validate_external_storage()
{
    oasys::ScopeLock scoplok(&sptr_clstatus_->lock_, __func__);

    if (sptr_clstatus_->mount_point_) {
        sptr_clstatus_->mount_pt_validated_ = validate_mount_point(sptr_clstatus_->storage_path_, sptr_clstatus_->validated_mount_pt_);
    } else {
        // flag the location as validated since it is not a mount point we need to check
        sptr_clstatus_->mount_pt_validated_ = true;
        sptr_clstatus_->validated_mount_pt_ = "/";
    }

    bool is_online = false;

    if (sptr_clstatus_->mount_pt_validated_) {
        sptr_clstatus_->storage_path_exists_ = create_storage_path(sptr_clstatus_->storage_path_);

        if (sptr_clstatus_->storage_path_exists_) {
            update_restage_cl_state(RESTAGE_CL_STATE_ONLINE);
            is_online = true;
        }
    }

    // update_volume_stats may set the state to FULL and also return true
    // so don't override to ONLINE below
    is_online = (is_online && update_volume_stats());
    is_online = (is_online && scan_external_storage());

    if (!is_online) {
        update_restage_cl_state(RESTAGE_CL_STATE_ERROR);
    }

    if (!registered_with_bard_) {
        sptr_bard_->register_restage_cl(sptr_clstatus_);
        registered_with_bard_ = true;
    }
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::validate_mount_point(std::string& storage_path, std::string& validated_mount_pt)
{
    bool result = false;

    // open the system file of mounted drives
    FILE* fp_mounts = setmntent("/proc/mounts", "r");
    if (!fp_mounts) {
        fp_mounts = setmntent("/etc/mtab", "r");
    }

    if (!fp_mounts) {
        log_err("Error opening /proc/mounts or /etc/mtab to get list of mounted drives");
    } else {
        char buf[1024];
        struct mntent entry;
        struct mntent* ptr_entry;
        const char* mount_path;

        while ((ptr_entry = getmntent_r(fp_mounts, &entry, buf, sizeof(buf)))) {
            mount_path = ptr_entry->mnt_dir;

            int mount_path_len = strlen(mount_path);
            int storage_path_len = storage_path.length();

            // skip the root volume "/" which has length of 1 and matches everything
            if ((mount_path_len > 1) && (storage_path_len >= mount_path_len)) {
                // check for a partial match with a match length greater than 1
                if (0 == storage_path.compare(0, mount_path_len, mount_path)) {
                    if (mount_path_len == storage_path_len) {
                        // exact match - using the while drive for storage
                        result = true;
                        validated_mount_pt = mount_path;
                    } else {
                        // we have a match up to the length of the mount path...
                        // the next character in the storage path must be a slash for a true match
                        result = (storage_path[mount_path_len] == '/');

                        if (result) {
                            // the storage_path_ is on a mounted drive
                            validated_mount_pt = mount_path;
                            break;
                        }
                    }
                }
            }
        }

        // close the system file of mounted drives
        endmntent(fp_mounts);
    }

    if (!result) {
        validated_mount_pt.clear();
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::update_volume_stats()
{
    bool result = false;

    struct statvfs vol_stats;

    errno = 0;
    int retval = statvfs(sptr_clstatus_->storage_path_.c_str(), &vol_stats);

    while ((retval != 0) && (errno == EINTR)) {
        errno = 0;
        retval = statvfs(sptr_clstatus_->storage_path_.c_str(), &vol_stats);
    }

    if (retval != 0) {
        switch (errno) {
            case EACCES:
                    log_err("%s: EACCESS - Insufficient permission to search path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case EFAULT:
                    log_err("%s: EFAULT - Invalid address supplied to statvfs. path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case EIO:
                    log_err("%s: EIO - I/O error while reading from file system. path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case ELOOP:
                    log_err("%s: ELOOP - Too many symbolic links in path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case ENAMETOOLONG:
                    log_err("%s: ENAMETOOLONG - Path is too long: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case ENOENT:
                    log_err("%s: ENOENT - File does not exist. path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case ENOMEM:
                    log_err("%s: ENOMEM - Insufficient kernel memory. path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;
            case EOVERFLOW:
                    log_err("%s: EOVERFLOW - Some values were too large to return by statvfs. path: %s", __func__, sptr_clstatus_->storage_path_.c_str());
                    break;

            default:
                    char buf[128];
                    strerror_r(errno, buf, sizeof(buf));
                    log_err("%s: Unexpected error: %s; path: %s", __func__, buf, sptr_clstatus_->storage_path_.c_str());
                    break;


        }

        // set tracking values to zero
        sptr_clstatus_->vol_total_space_ = 0;
        sptr_clstatus_->vol_space_available_ = 0;
    } else {
        result = true;

        sptr_clstatus_->vol_total_space_ = (size_t) vol_stats.f_frsize * (size_t) vol_stats.f_blocks;
        sptr_clstatus_->vol_space_available_ = (size_t) vol_stats.f_bsize * (size_t) vol_stats.f_bavail;

        if (sptr_clstatus_->vol_space_available_ < sptr_params_->min_disk_space_available_) {
            update_restage_cl_state(RESTAGE_CL_STATE_FULL);
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::create_storage_path(std::string& path)
{

    bool result = false;

    errno = 0;
    int retval = mkdir(path.c_str(), FILEMODE_ALL);

    if ((retval == 0) || (errno == EEXIST)) {
        // success
        result = true;
    } else if (errno != ENOENT) {
        // unable to create a directory - fall through to return false
        log_err("Error creating storage path: %s - (%d) %s", path.c_str(), errno, strerror(errno));
    } else {
        // ENOENT - higher level directory does not exist yet
        size_t pos = path.find_last_of('/');
        
        if (pos != std::string::npos) {
            // try to create the next higher directory
            std::string higher_path = path.substr(0, pos);

            result = create_storage_path(higher_path);

            if (result) {
                result = false;

                errno = 0;
                retval = mkdir(path.c_str(), FILEMODE_ALL);

                if (retval == 0) {
                    // success
                    result = true;
                } else {
                    log_err("Error creating storage path after higher directory was created: %s - (%d) %s", path.c_str(), errno, strerror(errno));
                }
            } else {
                log_err("Error creating higher directory: %s - aborting", path.c_str());
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::create_storage_subdirectory(std::string& dirname, std::string& full_path)
{
    full_path = sptr_clstatus_->storage_path_;
    full_path.append("/");
    full_path.append(dirname);

    return create_storage_path(full_path);
}

//----------------------------------------------------------------------
std::string
RestageConvergenceLayer::ExternalStorageController::format_bundle_dir_name(Bundle* bundle)
{
    if (bundle->bard_restage_by_src()) {
        return format_bundle_dir_name_by_src(bundle);
    } else {
        return format_bundle_dir_name_by_dst(bundle);
    }
}

//----------------------------------------------------------------------
std::string
RestageConvergenceLayer::ExternalStorageController::format_bundle_dir_name_by_src(Bundle* bundle)
{
    std::string dirname(TOKEN_SRC);

    std::string scheme_and_token;
    std::string nodename;
    std::string service;

    eid_to_tokens(bundle->source(), scheme_and_token, nodename, service);

    dirname.append(scheme_and_token);
    dirname.append(nodename);

    return dirname;
}

//----------------------------------------------------------------------
std::string
RestageConvergenceLayer::ExternalStorageController::format_bundle_dir_name_by_dst(Bundle* bundle)
{
    std::string dirname(TOKEN_DST);

    std::string scheme_and_token;
    std::string nodename;
    std::string service;

    eid_to_tokens(bundle->dest(), scheme_and_token, nodename, service);

    dirname.append(scheme_and_token);
    dirname.append(nodename);

    return dirname;
}

//----------------------------------------------------------------------
std::string
RestageConvergenceLayer::ExternalStorageController::format_bundle_dir_name(bard_quota_type_t quota_type, 
                                                                           bard_quota_naming_schemes_t scheme, 
                                                                           std::string& nodename)
{
    std::string dirname;

    // starts with the quota type
    if (quota_type == BARD_QUOTA_TYPE_SRC) {
        dirname = TOKEN_SRC;
    } else {
        dirname = TOKEN_DST;
    }

    // next is the scheme type
    switch (scheme) {
        case BARD_QUOTA_NAMING_SCHEME_IPN:
            dirname.append(TOKEN_IPN);
            break;

        case BARD_QUOTA_NAMING_SCHEME_DTN:
            dirname.append(TOKEN_DTN);
            break;

        case BARD_QUOTA_NAMING_SCHEME_IMC:
            dirname.append(TOKEN_IMC);
            break;

        default:
            break;

    }
    
    dirname.append(nodename);

    return dirname;
}

//----------------------------------------------------------------------
std::string
RestageConvergenceLayer::ExternalStorageController::format_bundle_filename(Bundle* bundle)
{
    // underscore is a field separator -- could cause issues with the dtn scheme??
    std::string filename;

    std::string token1;
    std::string token2;
    std::string token3;

    // add the src EID field
    eid_to_tokens(bundle->source(), token1, token2, token3);

    filename.append(TOKEN_SRC);
    filename.append(token1); // scheme includes EID_FIELD_SEPARATOR
    filename.append(token2); // nodename
    filename.append(EID_FIELD_SEPARATOR_STR);
    filename.append(token3);  // service


    // add the dst EID field
    eid_to_tokens(bundle->dest(), token1, token2, token3);

    filename.append(TOKEN_DST_EMBEDDED);
    filename.append(token1); // scheme includes EID_FIELD_SEPARATOR
    filename.append(token2); // nodename
    filename.append(EID_FIELD_SEPARATOR_STR);
    filename.append(token3);  // service

    // add the bundle timestamp field
    bts_to_tokens(bundle, token1, token2, token3);

    filename.append(TOKEN_BTS_EMBEDDED);
    filename.append(token1);   // human friendly date/time
    filename.append(FIELD_SEPARATOR_STR);
    filename.append(token2);   // creation timestamp dtn time
    filename.append(FIELD_SEPARATOR_STR);
    filename.append(token3);   // creation timestamp sequence number

    // add the fragmentation fields
    frag_to_tokens(bundle, token1, token2);

    filename.append(TOKEN_FRG_EMBEDDED);
    filename.append(token1);   // fragment offset
    filename.append(FIELD_SEPARATOR_STR);
    filename.append(token2);   // fragment length

    // add the payload length fields
    payload_to_tokens(bundle, token1);

    filename.append(TOKEN_PAY_EMBEDDED);
    filename.append(token1);   // payload length (or original if frag)

    // add the expiration fields
    exp_to_tokens(bundle, token1, token2);

    filename.append(TOKEN_EXP_EMBEDDED);
    filename.append(token1);   // human friendly date/time
    filename.append(FIELD_SEPARATOR_STR);
    filename.append(token2);   // expiration timestamp dtn time

    return filename;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::eid_to_tokens(const EndpointID& eid, std::string& scheme_token,
                                                                  std::string& nodename, std::string& service)
{
    // determine the scheme type
    if (eid.scheme() == IPNScheme::instance()) {
        scheme_token = TOKEN_IPN;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IPNScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        nodename = std::to_string(u_nodenum);
        service = std::to_string(u_service);

    } else if (eid.scheme() == DTNScheme::instance()) {
        scheme_token = TOKEN_DTN;
        nodename = eid.uri().authority();
        service = eid.uri().path();
    } else if (eid.scheme() == IMCScheme::instance()) {
        scheme_token = TOKEN_IMC;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IMCScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        nodename = std::to_string(u_nodenum);
        service = std::to_string(u_service);

    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::bts_to_tokens(Bundle* bundle, std::string& human_readable,
                                                                  std::string& dtn_time, std::string& seq_num)
{
    BundleTimestamp bts = bundle->creation_ts();

    dtn_time = std::to_string(bts.secs_or_millisecs_);
    seq_num = std::to_string(bts.seqno_);

    time_t timer = bundle->creation_time_secs() + BundleTimestamp::TIMEVAL_CONVERSION_SECS;
    struct tm tmval_buf;
    struct tm* tmval;

    char buf[24];

    tzset();
    tmval = gmtime_r(&timer, &tmval_buf);

    strftime(buf, sizeof(buf), "%Y-%j-%H%M%S", tmval);
    human_readable = buf;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::frag_to_tokens(Bundle* bundle,
                                                                  std::string& frag_offset, std::string& frag_len)
{
    frag_offset = std::to_string(bundle->frag_offset());
    frag_len = std::to_string(bundle->frag_length());
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::payload_to_tokens(Bundle* bundle, std::string& payload_len)
{
    if (bundle->is_fragment()) {
        payload_len = std::to_string(bundle->orig_length());
    } else {
        payload_len = std::to_string(bundle->payload().length());
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::exp_to_tokens(Bundle* bundle,
                                                                  std::string& human_readable, std::string& dtn_time)
{
    time_t timer = bundle->creation_time_secs() + bundle->expiration_secs();

    dtn_time = std::to_string(timer);

    // adjust from DTN time to unix time
    timer += BundleTimestamp::TIMEVAL_CONVERSION_SECS;

    struct tm tmval_buf;
    struct tm* tmval;

    char buf[24];

    tzset();
    tmval = gmtime_r(&timer, &tmval_buf);

    strftime(buf, sizeof(buf), "%Y-%j-%H%M%S", tmval);
    human_readable = buf;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_dirname(std::string dirname, bard_quota_type_t& quota_type,
                                                                  bard_quota_naming_schemes_t& scheme,
                                                                  std::string& nodename, size_t& node_number)
{
    bool result = parse_dname_quota_type(dirname, quota_type);
    if (result) {
        result = parse_dname_node(dirname, scheme, nodename, node_number);
    }
    return result;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_dname_quota_type(std::string& dirname, 
                                                                           bard_quota_type_t& quota_type)
{
    bool result = true;

    quota_type = BARD_QUOTA_TYPE_UNDEFINED;

    // Does the dirname begin properly?
    if (0 == dirname.compare(0, TOKEN_SRC_LEN, TOKEN_SRC)) {
        quota_type = BARD_QUOTA_TYPE_SRC;
    } else if (0 == dirname.compare(0, TOKEN_DST_LEN, TOKEN_DST)) {
        quota_type = BARD_QUOTA_TYPE_DST;
    } else {
        return false;
    }

    if (result) {
        // strip off the first 4 characters of the string
        // (both tokens are the same length)
        dirname = dirname.substr(TOKEN_SRC_LEN);
    }

    return result;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_dname_node(std::string& dirname, 
                                                                     bard_quota_naming_schemes_t& scheme,
                                                                     std::string& nodename,
                                                                     size_t& node_number)
{
    bool result = true;

    scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
    node_number = 0;

    // Does the dirname begin properly?
    if (0 == dirname.compare(0, TOKEN_IPN_LEN, TOKEN_IPN)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IPN;
    } else if (0 == dirname.compare(0, TOKEN_DTN_LEN, TOKEN_DTN)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_DTN;
    } else if (0 == dirname.compare(0, TOKEN_IMC_LEN, TOKEN_IMC)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IMC;
    } else {
        result = false;
    }

    if (result) {
        // strip off the first 4 characters of the string
        // (the token lengths are all the same)
        nodename = dirname.substr(TOKEN_IPN_LEN);

        if (scheme != BARD_QUOTA_NAMING_SCHEME_DTN) {
            char* end;
            node_number = std::strtoull(nodename.c_str(), &end, 10);
            if (*end != '\0') {
                // extraneous characters after the number - abort
                result = false;
            }
        }
    }

    return result;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_filename(std::string filename, SPtr_BundleFileDesc& sptr_bfd)
{
    bool result = false;

    // create a new object optimistically expecting this to be a valid filename
    sptr_bfd = std::make_shared<BundleFileDesc>();

    // store the original filename before starting to whack it
    sptr_bfd->filename_ = filename;

    // note that these functions strip off the parsed data from the filename string 
    result = parse_fname_src_eid(filename, sptr_bfd.get());
    if (result) {
        result = parse_fname_dst_eid(filename, sptr_bfd.get());
    }
    if (result) {
        result = parse_fname_bts(filename, sptr_bfd.get());
    }
    if (result) {
        result = parse_fname_frag(filename, sptr_bfd.get());
    }
    if (result) {
        result = parse_fname_payload(filename, sptr_bfd.get());
    }
    if (result) {
        result = parse_fname_exp(filename, sptr_bfd.get());
    }

    if (!result) {
        sptr_bfd.reset();
    } else {
        //log_always("%s: got good exp", __func__);
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_src_eid(std::string& filename,
                                                                        BundleFileDesc* bfd_ptr)
{
    bool result = true;

    if (0 != filename.compare(0, TOKEN_SRC_LEN, TOKEN_SRC)) {
        result = false;
    } else {
        filename = filename.substr(TOKEN_SRC_LEN);

        bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        std::string nodename;
        size_t node_number = 0;
        std::string service;
        result = parse_fname_eid(filename, scheme, nodename, node_number, service);

        if (result) {
            bfd_ptr->src_eid_scheme_ = scheme;
            bfd_ptr->src_eid_nodename_ = nodename;
            bfd_ptr->src_eid_node_number_ = node_number;
            bfd_ptr->src_eid_service_ = service;
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_dst_eid(std::string& filename,
                                                                        BundleFileDesc* bfd_ptr)
{
    bool result = true;

    if (0 != filename.compare(0, TOKEN_DST_EMBEDDED_LEN, TOKEN_DST_EMBEDDED)) {
        result = false;
    } else {
        filename = filename.substr(TOKEN_DST_EMBEDDED_LEN);

        
        bard_quota_naming_schemes_t scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        std::string nodename;
        size_t node_number = 0;
        std::string service;
        result = parse_fname_eid(filename, scheme, nodename, node_number, service);

        if (result) {
            bfd_ptr->dst_eid_scheme_ = scheme;
            bfd_ptr->dst_eid_nodename_ = nodename;
            bfd_ptr->dst_eid_node_number_ = node_number;
            bfd_ptr->dst_eid_service_ = service;
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_eid(std::string& filename, 
                                                                    bard_quota_naming_schemes_t& scheme,
                                                                    std::string& nodename,
                                                                    size_t& node_number,
                                                                    std::string service)
{
    bool result = true;

    node_number = 0;

    // pieces of the EID are separated by dashes  
    if (0 == filename.compare(0, TOKEN_IPN_LEN, TOKEN_IPN)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IPN;
    } else if (0 == filename.compare(0, TOKEN_DTN_LEN, TOKEN_DTN)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_DTN;
    } else if (0 == filename.compare(0, TOKEN_IMC_LEN, TOKEN_IMC)) {
        scheme = BARD_QUOTA_NAMING_SCHEME_IMC;
    } else {
        scheme = BARD_QUOTA_NAMING_SCHEME_UNDEFINED;
        result = false;
    }

    if (result) {
        // strip off the first 4 characters of the string
        // (all of these tokens are the same length)
        filename = filename.substr(TOKEN_IPN_LEN);

        size_t pos = filename.find(EID_FIELD_SEPARATOR_STR);

        if (pos == std::string::npos) {
            result = false;
        } else {
            // extract the node name
            nodename = filename.substr(0, pos);

            // strip off the node name and field separator
            filename = filename.substr(pos+1);

            // service is the last piece of the EID so the next separator will be a FIELD_SEPARATOR_CHAR
            pos = filename.find(FIELD_SEPARATOR_CHAR);

            if (pos == std::string::npos) {
                result = false;
            } else {
                // extract the srevice
                service = filename.substr(0, pos);

                // strip off the service but leave the separator character
                filename = filename.substr(pos);

                if (scheme != BARD_QUOTA_NAMING_SCHEME_DTN) {
                    char* end;
                    node_number = std::strtoull(nodename.c_str(), &end, 10);
                    if (*end != '\0') {
                        // extraneous characters after the number - abort
                        result = false;
                    }
                }
            }
        }
    }

    return result;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_bts(std::string& filename, BundleFileDesc* bfd_ptr)
{
    bool result = true;

    // string must start with the BTS token
    if (0 != filename.compare(0, TOKEN_BTS_EMBEDDED_LEN, TOKEN_BTS_EMBEDDED)) {
        result = false;
    }

    if (result) {
        std::string bts_seconds;
        std::string bts_seq_num;

        // skip the human readable timestamp and next char should be a FIELD_SEPARATOR_CHAR
        size_t skip = TOKEN_BTS_EMBEDDED_LEN + HUMAN_TIMESTAMP_LEN;
        if (filename[skip] != FIELD_SEPARATOR_CHAR) {
            result = false;
        } else {
            ++skip; // also skip the FIELD_SEPARATOR that we now know is there
            filename = filename.substr(skip);

            size_t pos = filename.find(FIELD_SEPARATOR_STR);

            if (pos == std::string::npos) {
                result = false;
            } else {
                bts_seconds = filename.substr(0, pos);

                // skip to the timestamp sequence number (skip the time and the separator (+1))
                filename = filename.substr(pos + 1);

                pos = filename.find(FIELD_SEPARATOR_STR);

                if (pos == std::string::npos) {
                    result = false;
                } else {
                    bts_seq_num = filename.substr(0, pos);

                    // skip just the sequence number and leave the FIELD_SEPARATOR for the next parser
                    filename = filename.substr(pos);
                }
            }
    
            if (result) {
                // convert the string to numeric values
                char* end;
                bfd_ptr->bts_secs_or_millisecs_ = std::strtoull(bts_seconds.c_str(), &end, 10);
                if (*end != '\0') {
                    // extraneous characters after the number - abort
                    result = false;
                }

                bfd_ptr->bts_seq_num_ = std::strtoull(bts_seq_num.c_str(), &end, 10);
                if (*end != '\0') {
                    // extraneous characters after the number - abort
                    result = false;
                }
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_frag(std::string& filename, BundleFileDesc* bfd_ptr)
{
    bool result = true;

    // string must start with the FRG token
    if (0 != filename.compare(0, TOKEN_FRG_EMBEDDED_LEN, TOKEN_FRG_EMBEDDED)) {
        result = false;
    }

    if (result) {
        // skip to the frag offset
        filename = filename.substr(TOKEN_FRG_EMBEDDED_LEN);

        std::string frag_offset;
        std::string frag_length;

        size_t pos = filename.find(FIELD_SEPARATOR_STR);

        if (pos == std::string::npos) {
            result = false;
        } else {
            frag_offset = filename.substr(0, pos);

            // skip to the frag length (skip the offset and the separator (+1))
            filename = filename.substr(pos + 1);

            pos = filename.find(FIELD_SEPARATOR_STR);

            if (pos == std::string::npos) {
                result = false;
            } else {
                frag_length = filename.substr(0, pos);

                // skip just the frag length and leave the FIELD_SEPARATOR for the next parser
                filename = filename.substr(pos);
            }
        }
    
        if (result) {
            // convert the string to numeric values
            char* end;
            bfd_ptr->frag_offset_ = std::strtoull(frag_offset.c_str(), &end, 10);
            if (*end != '\0') {
                // extraneous characters after the number - abort
                result = false;
            }

            bfd_ptr->frag_length_ = std::strtoull(frag_length.c_str(), &end, 10);
            if (*end != '\0') {
                // extraneous characters after the number - abort
                result = false;
            }


            bfd_ptr->is_frag_ = (bfd_ptr->frag_length_ > 0);
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_payload(std::string& filename, BundleFileDesc* bfd_ptr)
{
    bool result = true;

    // string must start with the PAY token
    if (0 != filename.compare(0, TOKEN_PAY_EMBEDDED_LEN, TOKEN_PAY_EMBEDDED)) {
        result = false;
    }

    if (result) {
        // skip to the payload length
        filename = filename.substr(TOKEN_PAY_EMBEDDED_LEN);

        std::string payload_len;

        size_t pos = filename.find(FIELD_SEPARATOR_STR);

        if (pos == std::string::npos) {
            result = false;
        } else {
            payload_len = filename.substr(0, pos);

            // skip just the payload length and leave the FIELD_SEPARATOR for the next parser
            filename = filename.substr(pos);
        }
    
        if (result) {
            // convert the string to numeric values
            char* end;
            size_t paylen = std::strtoull(payload_len.c_str(), &end, 10);
            if (*end != '\0') {
                // extraneous characters after the number - abort
                result = false;
            }

            if (bfd_ptr->is_frag_) {
                bfd_ptr->orig_payload_length_ = paylen;
                bfd_ptr->payload_length_ = bfd_ptr->frag_length_;
            } else {
                bfd_ptr->orig_payload_length_ = paylen;
                bfd_ptr->payload_length_ = paylen;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::parse_fname_exp(std::string& filename, BundleFileDesc* bfd_ptr)
{
    bool result = true;

    // string must start with the EXP token
    if (0 != filename.compare(0, TOKEN_EXP_EMBEDDED_LEN, TOKEN_EXP_EMBEDDED)) {
        result = false;
    }

    if (result) {
        // skip over the human readable date to the DTN expiration time 
        size_t skip = TOKEN_BTS_EMBEDDED_LEN + HUMAN_TIMESTAMP_LEN;
        if (filename[skip] != FIELD_SEPARATOR_CHAR) {
            result = false;
        } else {
            ++skip; // also skip the FIELD_SEPARATOR that we now know is there
            filename = filename.substr(skip);

            // expiration should be the last field in the filename
            // convert the string to numeric values
            char* end;
            bfd_ptr->exp_seconds_ = std::strtoull(filename.c_str(), &end, 10);
            if (*end != '\0') {
                // extraneous characters after the number - abort
                result = false;
            }
        }
    }

    return result;
}


//----------------------------------------------------------------------
// local global values for scandir64 filter work-arounds
struct stat stat_buf;
std::string scandir64_dirpath;
std::string scandir64_filepath;

int scandir64_filter_dirs(const struct dirent64* eptr) {
    int result = 1;

    std::string fullpath = scandir64_dirpath + eptr->d_name;

    // workaround non-ext file systems such as xfs do not set the proper d_type value
    if (0 == eptr->d_type) {
        // errors will fall through and entry will be ignored
        if (-1 != stat(fullpath.c_str(), &stat_buf)) {
            // overwrite the unknown d_type if appropriate
            switch (stat_buf.st_mode & S_IFMT)
            {
                case S_IFDIR:
                    ((struct dirent64*) eptr)->d_type = DT_DIR;
                    break;

                case S_IFREG:
                    ((struct dirent64*) eptr)->d_type = DT_REG;
                    break;

                default: // ignore
                    break;
            }
        }
    }


    // only interested in directories with this filter
    if (DT_DIR != eptr->d_type) {
      result = 0;  // not a directory so skip it
    }

    return result;
}

int scandir64_filter_files(const struct dirent64* eptr) {
    int result = 1;

    std::string fullpath = scandir64_filepath + eptr->d_name;

    // workaround non-ext file systems such as xfs do not set the proper d_type value
    if (0 == eptr->d_type) {
        // errors will fall through and entry will be ignored
        if (-1 != stat(fullpath.c_str(), &stat_buf)) {
            // overwrite the unknown d_type if appropriate
            switch (stat_buf.st_mode & S_IFMT)
            {
                case S_IFDIR:
                    ((struct dirent64*) eptr)->d_type = DT_DIR;
                    break;

                case S_IFREG:
                    ((struct dirent64*) eptr)->d_type = DT_REG;
                    break;

                default: // ignore
                    break;
            }
        }
    }


    // only interested in regular files with this filter
    if (DT_REG != eptr->d_type) {
      result = 0;  // not a regular file so skip it
    }

    return result;
}


//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::scan_external_storage()
{
    if (!sptr_clstatus_->storage_path_exists_) {
        return false;
    }

    std::string path_base = sptr_clstatus_->storage_path_ + "/";
    std::string dirpath;


    log_always("scanning external storage: %s", sptr_clstatus_->storage_path_.c_str());


    // get a list of the directories at the top level
    bool result = true;

    std::string dirname;
    bard_quota_type_t quota_type;
    bard_quota_naming_schemes_t scheme;
    std::string nodename;
    size_t node_number;

    struct dirent64** dirfiles = nullptr;


    pause_all_activity(); // prevent bundle restaging and reloading

    scandir64_dirpath = path_base;

    int dcount = scandir64(sptr_clstatus_->storage_path_.c_str(), &dirfiles, scandir64_filter_dirs, nullptr);

    for (int ix=0; ix<dcount; ++ix) {
        dirname = dirfiles[ix]->d_name;

        // got what we need so we can free this entry
        free(dirfiles[ix]);

        if (parse_dirname(dirname, quota_type, scheme, nodename, node_number)) {
            // this is a valid restage directory

            dirpath = path_base + dirname;

            log_always("scanning storage subdirectory: %s", dirpath.c_str());

             scan_restage_subdirectory(dirname, dirpath, quota_type);

        } else {
            // not a directory that we care about - skip it
            log_debug("%s: skipping scanning directory: %s", __func__, dirname.c_str());
        }
    }

    free(dirfiles);

    if (!external_storage_scanned_) {
        // only update the BARD on the first successful scan
        update_bard_usage_stats();
    }

    external_storage_scanned_ = true;

    if (!perform_rescan_) {
        resume_all_activity();
    }

    return result;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::pause_all_activity()
{
    if (qptr_restager_) {
        qptr_restager_->pause();
    }
    if (qptr_reloader_) {
        qptr_reloader_->pause();
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::resume_all_activity()
{
    if (qptr_restager_) {
        qptr_restager_->resume();
    }
    if (qptr_reloader_) {
        qptr_reloader_->resume();
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::update_bard_usage_stats()
{
    SPtr_RestageDirStatistics sptr_dir_stats;

    RestageDirStatisticsMapIter stats_iter = dir_stats_map_.begin();

    while (stats_iter != dir_stats_map_.end()) {
        sptr_dir_stats = stats_iter->second;

        sptr_bard_->update_restage_usage_stats(sptr_dir_stats->quota_type_,
                                              sptr_dir_stats->scheme_,
                                              sptr_dir_stats->nodename_,
                                              sptr_dir_stats->num_files_,
                                              sptr_dir_stats->total_size_);
        ++stats_iter;
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::scan_restage_subdirectory(std::string& dirname,
                                                                              std::string& dirpath,
                                                                              bard_quota_type_t quota_type)
{
    // map of files for the directory currently being traversed
    SPtr_BundleFileDescMap sptr_bfd_map;

    // this is a valid restage directory
    // add it to the map if it doesn't already exist
    RestageDirMapIter dir_iter = restage_dir_map_.find(dirname);
    if (dir_iter == restage_dir_map_.end()) {
        sptr_bfd_map = std::make_shared<BundleFileDescMap>();
        restage_dir_map_[dirname] = sptr_bfd_map;
    } else {
        sptr_bfd_map = dir_iter->second;
    }

    std::string filename;
    SPtr_BundleFileDesc sptr_bfd;

    int found = 0;
    struct stat stat_buf;
    struct dirent64** dirfiles = nullptr;

    scandir64_filepath = dirpath + "/";

    int dcount = scandir64(dirpath.c_str(), &dirfiles, scandir64_filter_files, nullptr);

    for (int ix=0; ix<dcount; ++ix) {
        filename = dirfiles[ix]->d_name;

        // got what we need so we can free this entry
        free(dirfiles[ix]);

        sptr_bfd.reset();
        if (parse_filename(filename, sptr_bfd)) {
            // found a good bundle file - add it to the list
            ++found;

            BundleFileDescMapIter file_iter = sptr_bfd_map->find(filename);

            if (file_iter == sptr_bfd_map->end()) {
                sptr_bfd->filename_ = filename;
                sptr_bfd->quota_type_ = quota_type;

                // this is a new file - add it
                sptr_bfd_map->insert(BundleFileDescPair(filename, sptr_bfd));

                // call stat to get the file information
                if (0 == stat(filename.c_str(), &stat_buf)) {
                    sptr_bfd->file_size_ = stat_buf.st_size;
                    sptr_bfd->disk_usage_ = stat_buf.st_blocks * 512;
                    sptr_bfd->file_creation_time_ = stat_buf.st_mtime;

                } else {
                    // file length not available - use the payload length
                    sptr_bfd->file_size_  = sptr_bfd->payload_length_;          // add some slop??
                    sptr_bfd->disk_usage_ = sptr_bfd->payload_length_;

                    struct timeval now;
                    ::gettimeofday(&now, 0);
                    sptr_bfd->file_creation_time_ = now.tv_sec;
                }

                // add this file to the statistics
                add_to_statistics(dirname, sptr_bfd.get());

            } else {
                //log_always("%s: ignoring duplicate file: %s", __func__, filename.c_str());
                // must be rescanning the directory - just ignore
            }

        } else {
            //log_always("%s: ignoring file: %s", __func__, filename.c_str());
        }
    }

    free(dirfiles);


    log_always("scanned subdirectory: %s  #files: %d  #bundles: %d", 
               dirname.c_str(), dcount, found);
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::add_to_statistics(const std::string& dirname, 
                                                                      const BundleFileDesc* bfd_ptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    SPtr_RestageDirStatistics sptr_dir_stats;
    RestageDirStatisticsMapIter stats_iter = dir_stats_map_.find(dirname);

    if (stats_iter == dir_stats_map_.end()) {
        sptr_dir_stats = std::make_shared<RestageDirStatistics>();
        
        sptr_dir_stats->dirname_ = dirname;
        sptr_dir_stats->quota_type_ = bfd_ptr->quota_type_;

        if (sptr_dir_stats->quota_type_ == BARD_QUOTA_TYPE_DST) {
            sptr_dir_stats->scheme_ = bfd_ptr->dst_eid_scheme_;
            sptr_dir_stats->nodename_ = bfd_ptr->dst_eid_nodename_;
            sptr_dir_stats->node_number_ = bfd_ptr->dst_eid_node_number_;
        } else {
            sptr_dir_stats->scheme_ = bfd_ptr->src_eid_scheme_;
            sptr_dir_stats->nodename_ = bfd_ptr->src_eid_nodename_;
            sptr_dir_stats->node_number_ = bfd_ptr->src_eid_node_number_;
        }

        dir_stats_map_[dirname] = sptr_dir_stats;

    } else {
        sptr_dir_stats = stats_iter->second;
    }

    // add to the directoary level stats
    sptr_dir_stats->num_files_ += 1;
    sptr_dir_stats->total_size_ += bfd_ptr->disk_usage_;


    // add to the local grand totals
    grand_total_num_files_ += 1;
    grand_total_num_bytes_ += bfd_ptr->disk_usage_;

    // update the status record that is shared with the BARD
    oasys::ScopeLock clstatus_scoplok(&sptr_clstatus_->lock_, __func__);

    sptr_clstatus_->disk_num_files_ = grand_total_num_files_;
    sptr_clstatus_->disk_quota_in_use_ = grand_total_num_bytes_;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::load_bfd_bundle_data(BundleFileDesc* bfd_ptr,
                                                                         Bundle* bundle)
{
    if (bundle->bard_restage_by_src()) {
        bfd_ptr->quota_type_ = BARD_QUOTA_TYPE_SRC;
    } else {
        bfd_ptr->quota_type_ = BARD_QUOTA_TYPE_DST;
    }

    load_bfd_eid_src_data(bfd_ptr, bundle->source());
    load_bfd_eid_dst_data(bfd_ptr, bundle->dest());

    // load the bundle timestamp data
    bfd_ptr->bts_secs_or_millisecs_ = bundle->creation_ts().secs_or_millisecs_;
    bfd_ptr->bts_seq_num_ = bundle->creation_ts().seqno_;

     bfd_ptr->payload_length_ = bundle->payload().length();

    // fragment data
    if (bundle->is_fragment()) {
        bfd_ptr->is_frag_ = true;
        bfd_ptr->frag_offset_ = bundle->frag_offset();
        bfd_ptr->frag_length_ = bundle->frag_length();
        bfd_ptr->orig_payload_length_ = bundle->orig_length();
    } else {
        // frag fields default suffice
        bfd_ptr->orig_payload_length_ = bundle->payload().length();
    }

    // bundle expiration
    bfd_ptr->exp_seconds_ = bundle->creation_time_secs() + bundle->expiration_secs();
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::load_bfd_eid_src_data(BundleFileDesc* bfd_ptr,
                                                                          const EndpointID& eid)
{
    // determine the scheme type
    if (eid.scheme() == IPNScheme::instance()) {
        bfd_ptr->src_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_IPN;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IPNScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        bfd_ptr->src_eid_node_number_ = u_nodenum;
        bfd_ptr->src_eid_nodename_ = std::to_string(u_nodenum);
        bfd_ptr->src_eid_service_ = std::to_string(u_service);

    } else if (eid.scheme() == DTNScheme::instance()) {
        bfd_ptr->src_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_DTN;
        bfd_ptr->src_eid_nodename_ = eid.uri().authority();
        bfd_ptr->src_eid_service_ = eid.uri().path();

    } else if (eid.scheme() == IMCScheme::instance()) {
        bfd_ptr->src_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_IMC;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IPNScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        bfd_ptr->src_eid_node_number_ = u_nodenum;
        bfd_ptr->src_eid_nodename_ = std::to_string(u_nodenum);
        bfd_ptr->src_eid_service_ = std::to_string(u_service);
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::load_bfd_eid_dst_data(BundleFileDesc* bfd_ptr,
                                                                          const EndpointID& eid)
{
    // determine the scheme type
    if (eid.scheme() == IPNScheme::instance()) {
        bfd_ptr->dst_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_IPN;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IPNScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        bfd_ptr->dst_eid_node_number_ = u_nodenum;
        bfd_ptr->dst_eid_nodename_ = std::to_string(u_nodenum);
        bfd_ptr->dst_eid_service_ = std::to_string(u_service);

    } else if (eid.scheme() == DTNScheme::instance()) {
        bfd_ptr->dst_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_DTN;
        bfd_ptr->dst_eid_nodename_ = eid.uri().authority();
        bfd_ptr->dst_eid_service_ = eid.uri().path();

    } else if (eid.scheme() == IMCScheme::instance()) {
        bfd_ptr->dst_eid_scheme_ = BARD_QUOTA_NAMING_SCHEME_IMC;

        uint64_t u_nodenum;
        uint64_t u_service;

        std::string ssp = eid.ssp();
        if (! IPNScheme::parse(ssp, &u_nodenum, &u_service)) {
            u_nodenum = 0;
            u_service = 0;
        }

        bfd_ptr->dst_eid_node_number_ = u_nodenum;
        bfd_ptr->dst_eid_nodename_ = std::to_string(u_nodenum);
        bfd_ptr->dst_eid_service_ = std::to_string(u_service);
    }
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::bundle_restaged(const std::string& dirname,
                                                                    const std::string& filename,
                                                                    Bundle* bundle, 
                                                                    size_t file_size, size_t disk_usage)
{
    // map of files for the directory currently being traversed
    SPtr_BundleFileDescMap sptr_bfd_map;

    oasys::ScopeLock scoplok(&lock_, __func__);

    // this is a valid restage directory
    // add it to the map if it doesn't already exist
    RestageDirMapIter dir_iter = restage_dir_map_.find(dirname);
    if (dir_iter == restage_dir_map_.end()) {
        sptr_bfd_map = std::make_shared<BundleFileDescMap>();
        restage_dir_map_[dirname] = sptr_bfd_map;
    } else {
        sptr_bfd_map = dir_iter->second;
    }


    SPtr_BundleFileDesc sptr_bfd = std::make_shared<BundleFileDesc>();

    sptr_bfd->filename_ = filename;
    sptr_bfd->file_size_ = file_size;
    sptr_bfd->disk_usage_ = disk_usage;

    struct timeval now;
    ::gettimeofday(&now, 0);
    sptr_bfd->file_creation_time_ = now.tv_sec;   // just added so this is close enough for our purposes

    load_bfd_bundle_data(sptr_bfd.get(), bundle);

    BundleFileDescMapIter file_iter = sptr_bfd_map->find(filename);

    if (file_iter == sptr_bfd_map->end()) {
        // this is a new file - add it
        sptr_bfd_map->insert(BundleFileDescPair(filename, sptr_bfd));

        // add this file to the statistics
        add_to_statistics(dirname, sptr_bfd.get());

        ++total_restaged_;

    } else {
        log_always("%s: ignoring duplicate file in ESC stats: %s", __func__, filename.c_str());
        // must be rescanning the directory - just ignore

        ++total_restage_dupes_ignored_;
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::bundle_restage_error()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    ++total_restage_errors_;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::bundle_reloaded(const std::string& dirname,
                                                                    BundleFileDescMap* bfd_map_ptr,
                                                                    const BundleFileDesc* bfd_ptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    ++total_reloaded_;

    // remove file from the statistics
    del_from_statistics(dirname, bfd_ptr);

    bfd_map_ptr->erase(bfd_ptr->filename_);
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::bundle_deleted(const std::string& dirname,
                                                                   BundleFileDescMap* bfd_map_ptr,
                                                                   const BundleFileDesc* bfd_ptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    ++total_deleted_;

    // remove file from the statistics
    del_from_statistics(dirname, bfd_ptr);

    bfd_map_ptr->erase(bfd_ptr->filename_);
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::set_cl_state(restage_cl_states_t new_state)
{
    update_restage_cl_state(new_state);
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::update_restage_cl_state(restage_cl_states_t new_state)
{
    bool send_notifications = false;

    oasys::ScopeLock scoplok(&sptr_clstatus_->lock_, __func__);

    if (new_state != sptr_clstatus_->cl_state_) {
        switch (new_state) {
        case RESTAGE_CL_STATE_ONLINE:
            send_notifications = ((sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_FULL) ||
                                  (sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_ERROR));

            sptr_clstatus_->cl_state_ = RESTAGE_CL_STATE_ONLINE;

            send_notifications = true;  //dzdebug
            log_always("Restage CL State transitioned to: %s", 
                       restage_cl_states_to_str(new_state));
            break;

        case RESTAGE_CL_STATE_FULL:
            send_notifications = true;

            sptr_clstatus_->cl_state_ = RESTAGE_CL_STATE_FULL;

            log_err("Restage CL State transitioned to: %s", 
                       restage_cl_states_to_str(new_state));
            break;

        case RESTAGE_CL_STATE_ERROR:
            send_notifications = true;

            sptr_clstatus_->cl_state_ = RESTAGE_CL_STATE_ERROR;

            log_err("Restage CL State transitioned to: %s", 
                       restage_cl_states_to_str(new_state));
            break;

        case RESTAGE_CL_STATE_DELETED:
            break;

        default:
            send_notifications = true;

            sptr_clstatus_->cl_state_ = RESTAGE_CL_STATE_UNDEFINED;

            log_err("Restage CL State transitioned to: %s", 
                       restage_cl_states_to_str(new_state));
            break;
        }
    }

    scoplok.unlock();

    if (send_notifications) {
        send_email_notifications(new_state);
    }
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::send_email_notifications(restage_cl_states_t new_state)
{
    (void) new_state;
    //
    if (sptr_params_->email_enabled_) {
        oasys::ScopeLock scoplok(&sptr_params_->sptr_emails_->lock_, __func__);

        if (sptr_params_->sptr_emails_->email_list_.size() > 0) {
            // kick off a new thread to send the emails
            Emailer* emailer = new Emailer(new_state, sptr_params_->sptr_emails_);
            emailer->start();
        }
    }
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::disk_quota_is_available(ssize_t space_needed)
{
    bool result = true;

    if (sptr_params_->disk_quota_ > 0) {
        size_t new_total = grand_total_num_bytes_ + space_needed;
        if (new_total > sptr_params_->disk_quota_) {
            result = false;

            // set the state to indicae the drive is full
            update_restage_cl_state(RESTAGE_CL_STATE_FULL);
        }
    }

    return result;
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::del_from_statistics(const std::string& dirname, 
                                                                        const BundleFileDesc* bfd_ptr)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    SPtr_RestageDirStatistics sptr_dir_stats;
    RestageDirStatisticsMapIter stats_iter = dir_stats_map_.find(dirname);

    if (stats_iter == dir_stats_map_.end()) {
        log_err("%s: dirname [%s] not found in list", __func__, dirname.c_str());
        return;
    }

    sptr_dir_stats = stats_iter->second;

    // subtract from the directory level stats
    bool stats_error = false;

    if (sptr_dir_stats->num_files_ > 0) {
        sptr_dir_stats->num_files_ -= 1;
    } else {
        stats_error = true;
    }

    if (sptr_dir_stats->total_size_ >= bfd_ptr->disk_usage_) {
        sptr_dir_stats->total_size_ -= bfd_ptr->disk_usage_;
    } else {
        stats_error = true;
    }

    if (stats_error) {
        log_err("%s - directory level statistics are out of sync for %s",
                  sptr_clstatus_->restage_link_name_.c_str(), dirname.c_str());

        // trigger a rescan??
    }


    // subtract from the local grand totals
    stats_error = false;

    if (grand_total_num_files_ > 0) {
        grand_total_num_files_ -= 1;
    } else {
        stats_error = true;
    }

    if (grand_total_num_bytes_ >= bfd_ptr->disk_usage_) {
        grand_total_num_bytes_ -= bfd_ptr->disk_usage_;
    } else {
        stats_error = true;
    }

    if (stats_error) {
        log_err("%s - grand total statistics are out of sync", 
                sptr_clstatus_->restage_link_name_.c_str());
        // trigger a rescan??
    }

    // update the status record that is shared with the BARD
    oasys::ScopeLock clstatus_scoplok(&sptr_clstatus_->lock_, __func__);

    sptr_clstatus_->disk_num_files_ = grand_total_num_files_;
    sptr_clstatus_->disk_quota_in_use_ = grand_total_num_bytes_;

    if (sptr_clstatus_->cl_state_ == RESTAGE_CL_STATE_FULL) {
        if (sptr_params_->disk_quota_ > 0) {
            if (sptr_params_->disk_quota_ > grand_total_num_bytes_) {
                size_t space_available = sptr_params_->disk_quota_ - grand_total_num_bytes_;

                if (space_available >= sptr_params_->min_quota_available_) {
                    update_restage_cl_state(RESTAGE_CL_STATE_ONLINE);
                }
            }
        } else {
            // quota was removed
            update_restage_cl_state(RESTAGE_CL_STATE_ONLINE);
        }
    }
}

//----------------------------------------------------------------------
int
RestageConvergenceLayer::ExternalStorageController::reload_all(size_t new_expiration)
{
    int num_dirs = 0;

    std::string dirname;
    std::string dir_path;
    std::string no_new_dest_eid;

    SPtr_BundleFileDescMap sptr_bfd_map;

    oasys::ScopeLock scoplok(&lock_, __func__);


    if (perform_rescan_) {
        return 0;
    }

    // Initiate a reload for each directory

    RestageDirMapIter dir_iter = restage_dir_map_.begin();

    while (dir_iter != restage_dir_map_.end()) {
        dirname = dir_iter->first;

        dir_path = sptr_clstatus_->storage_path_;
        dir_path.append("/");
        dir_path.append(dirname);

        sptr_bfd_map = dir_iter->second;

        qptr_reloader_->post(dirname, sptr_bfd_map, dir_path, new_expiration, no_new_dest_eid);

        ++num_dirs;
        ++dir_iter;
    }

    return num_dirs;
}



//----------------------------------------------------------------------
int
RestageConvergenceLayer::ExternalStorageController::reload(bard_quota_type_t quota_type, 
                                                           bard_quota_naming_schemes_t scheme, 
                                                           std::string& nodename, 
                                                           size_t new_expiration, 
                                                           std::string& new_dest_eid)
{
    int num_dirs = 0;

    std::string dirname = format_bundle_dir_name(quota_type, scheme, nodename);
    std::string dir_path;

    SPtr_BundleFileDescMap sptr_bfd_map;

    oasys::ScopeLock scoplok(&lock_, __func__);

    // Initiate a reload for each directory
    RestageDirMapIter dir_iter = restage_dir_map_.find(dirname);

    if (dir_iter != restage_dir_map_.end()) {
        dir_path = sptr_clstatus_->storage_path_;
        dir_path.append("/");
        dir_path.append(dirname);

        sptr_bfd_map = dir_iter->second;

        qptr_reloader_->post(dirname, sptr_bfd_map, dir_path, new_expiration, new_dest_eid);

        ++num_dirs;
    }

    return num_dirs;
}

//----------------------------------------------------------------------
int
RestageConvergenceLayer::ExternalStorageController::delete_restaged_bundles(bard_quota_type_t quota_type, 
                                                                            bard_quota_naming_schemes_t scheme, 
                                                                            std::string& nodename)
{
    int num_dirs = 0;

    std::string dirname = format_bundle_dir_name(quota_type, scheme, nodename);
    std::string dir_path;

    SPtr_BundleFileDescMap sptr_bfd_map;

    oasys::ScopeLock scoplok(&lock_, __func__);

    // Initiate a reload for each directory
    RestageDirMapIter dir_iter = restage_dir_map_.find(dirname);

    if (dir_iter != restage_dir_map_.end()) {
        dir_path = sptr_clstatus_->storage_path_;
        dir_path.append("/");
        dir_path.append(dirname);

        sptr_bfd_map = dir_iter->second;

        std::string dummy_dest;
        // -99 indicates to delete rather than reload
        qptr_reloader_->post(dirname, sptr_bfd_map, dir_path, -99, dummy_dest);

        ++num_dirs;
    }

    return num_dirs;
}

//----------------------------------------------------------------------
int
RestageConvergenceLayer::ExternalStorageController::delete_all_restaged_bundles()
{
    int num_dirs = 0;

    std::string dirname;
    std::string dir_path;
    std::string no_new_dest_eid;

    SPtr_BundleFileDescMap sptr_bfd_map;

    oasys::ScopeLock scoplok(&lock_, __func__);


    if (perform_rescan_) {
        return 0;
    }

    // Initiate a reload for each directory

    RestageDirMapIter dir_iter = restage_dir_map_.begin();

    while (dir_iter != restage_dir_map_.end()) {
        dirname = dir_iter->first;

        dir_path = sptr_clstatus_->storage_path_;
        dir_path.append("/");
        dir_path.append(dirname);

        sptr_bfd_map = dir_iter->second;

        // -99 indicates to delete rather than reload
        qptr_reloader_->post(dirname, sptr_bfd_map, dir_path, -99, no_new_dest_eid);

        ++num_dirs;
        ++dir_iter;
    }

    return num_dirs;
}



//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::pause_for_rescan()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    pause_all_activity(); // prevent bundle restaging and reloading
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::resume_after_rescan()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    resume_all_activity(); // resume bundle restaging and reloading
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::rescan()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    perform_rescan_ = true;
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::do_garbage_collection()
{
    if ((sptr_params_->days_retention_ == 0) && !sptr_params_->expire_bundles_) {
        return; // nothing to do
    }

    oasys::Time timer;
    timer.get_time();

    pause_all_activity(); // prevent bundle restaging and reloading

    std::string dirname;
    std::string dir_path;

    SPtr_BundleFileDescMap sptr_bfd_map;

    size_t num_deleted = 0;

    oasys::ScopeLock scoplok(&lock_, __func__);


    if (perform_rescan_) {
        return;
    }

    // Initiate a garbage collection for each directory

    RestageDirMapIter dir_iter = restage_dir_map_.begin();

    while (dir_iter != restage_dir_map_.end()) {
        dirname = dir_iter->first;

        dir_path = sptr_clstatus_->storage_path_;
        dir_path.append("/");
        dir_path.append(dirname);

        sptr_bfd_map = dir_iter->second;

        num_deleted += garbage_collect_dir(dirname, sptr_bfd_map.get(), dir_path);

        ++dir_iter;
    }

    resume_all_activity();

    if (num_deleted > 0) {
        size_t elapsed_secs = timer.elapsed_ms() / 1000;
        log_always("Garbage collection deleted %zu bundles/files in %zu seconds", num_deleted, elapsed_secs);
    }
}

//----------------------------------------------------------------------
size_t
RestageConvergenceLayer::ExternalStorageController::garbage_collect_dir(const std::string& dirname,
                                                                        BundleFileDescMap* bfd_map_ptr, 
                                                                        std::string dir_path)
{
    // scan through the list of files to see if any need to be deleted
    SPtr_BundleFileDesc sptr_bfd;
    std::string fullpath;

    // make sure the dir path includes a trailing slash
    dir_path += "/";

    size_t num_deleted = 0;

    bool delete_the_file;
    time_t bundle_exp_time;
    time_t file_exp_time;
    struct timeval now;


    time_t file_retention_secs = sptr_params_->days_retention_ * 86400;

    BundleFileDescMapIter del_iter;
    BundleFileDescMapIter iter = bfd_map_ptr->begin();


    while ((!should_stop()) && (iter != bfd_map_ptr->end())) {

        sptr_bfd = iter->second;

        delete_the_file = false;

        ::gettimeofday(&now, 0);

        // check first by the bundle expiration time
        if (sptr_params_->expire_bundles_) {
            bundle_exp_time = sptr_bfd->exp_seconds_ + BundleTimestamp::TIMEVAL_CONVERSION_SECS;
            delete_the_file = (bundle_exp_time <= now.tv_sec);
        }

        // check by the bundle retention time
        if (!delete_the_file && (file_retention_secs > 0)) {
            file_exp_time = sptr_bfd->file_creation_time_ + file_retention_secs;
            delete_the_file = (file_exp_time <= now.tv_sec);
        }

        if (delete_the_file) {
            fullpath = dir_path;
            fullpath.append(iter->first);

            // delete the file
            if (0 == unlink(fullpath.c_str())) {
                bard_quota_naming_schemes_t scheme;
                std::string nodename;
                bard_quota_type_t quota_type = sptr_bfd->quota_type_;

                if (quota_type == BARD_QUOTA_TYPE_DST) {
                    scheme = sptr_bfd->dst_eid_scheme_;
                    nodename = sptr_bfd->dst_eid_nodename_;
                } else {
                    scheme = sptr_bfd->src_eid_scheme_;
                    nodename = sptr_bfd->src_eid_nodename_;
                }

                sptr_bard_->restaged_bundle_deleted(quota_type, scheme, nodename, sptr_bfd->disk_usage_);

                ++num_deleted;
            } else {
                log_err("Unable to delete expired bundle/file: %s - %s",
                        fullpath.c_str(), strerror(errno));
            }
        }

        ++iter;

        if (delete_the_file) {
            bundle_deleted(dirname, bfd_map_ptr, sptr_bfd.get());

            ++num_deleted;
        }
    }


    return num_deleted;
}





//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Restager::Restager(SPtr_ExternalStorageController sptr_esc,
                                                                      const std::string& link_name,
                                                                      const ContactRef& contact,
                                                                      SPtr_RestageCLParams sptr_params)
    : Logger("RestageCL:ESC:Restager",
             "/dtn/cl/%s/restager", link_name.c_str()),
      Thread("RestageCL::ESC::Restager"),
      sptr_esc_(sptr_esc),
      contact_(contact.object(), "RestageCL::ESC::Restager"),
      link_(contact_->link()),
      sptr_params_(sptr_params)
{
    file_.logpathf("restage/file");
}


//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Restager::~Restager()
{
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Restager::shutdown()
{
    // allow queued bundles to be processed
    log_always("Retage CL (%s) shutting down - wait for %zu bundles to be processed", 
               link_->name(), link_->queue()->size());

    while (!link_->queue()->empty()) {
        usleep(100000);
    }

    set_should_stop();

    while (!is_stopped()) {
        usleep(100000);
    }

    // release the shared pointer to the parent ESC
    sptr_esc_.reset();
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Restager::run()
{
    char threadname[16] = "Restager";
    pthread_setname_np(pthread_self(), threadname);

    while (!should_stop()) {
        // check to see if there is a bundle to process
        if (paused_ || !check_for_bundle()) {
            usleep(100000);
        } else {
            usleep(10);
        }
    }
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::Restager::check_for_bundle()
{
    BundleRef bref = link_->queue()->front();

    if (bref.object() == nullptr) {
        return false;
    }

    ssize_t len = process_bundle(bref);

    if (len > 0) {
        link_->del_from_queue(bref);
        link_->add_to_inflight(bref);
        BundleDaemon::post(
            new BundleRestagedEvent(bref.object(), contact_, link_, len, true, false));
    } else {
        link_->del_from_queue(bref);
        BundleDaemon::post(
            new BundleRestagedEvent(bref.object(), contact_, link_, 0, false, false));
    }

    return true;
}

//----------------------------------------------------------------------
ssize_t
RestageConvergenceLayer::ExternalStorageController::Restager::process_bundle(BundleRef& bref)
{
    Bundle* bundle = bref.object();


    std::string fullpath;
    std::string dirname = sptr_esc_->format_bundle_dir_name(bundle);
    std::string filename = sptr_esc_->format_bundle_filename(bundle);

    if (!sptr_esc_->create_storage_subdirectory(dirname, fullpath)) {
        log_err("error creating subdirectoy: %s", fullpath.c_str());
        return 0;
    }

    fullpath.append("/");
    fullpath.append(filename);



    bool error_writing_file = false;
    int err = 0;
    int open_errno = 0;
    err = file_.open(fullpath.c_str(), O_EXCL | O_CREAT | O_RDWR,
                     FILEMODE_ALL, &open_errno);
   
    if (err < 0 && open_errno == EEXIST)
    {
        log_always("not restaging duplicate bundle - file: %s",
                 fullpath.c_str());
        return bundle->payload().length();  // duplicate bundle

    } else if (err < 0) {
        log_err("error creating restage file: %s - %s",
                 fullpath.c_str(), strerror(errno));

        return 0;
    }


    size_t total_len = 0;

    do {   // just to limit the scope of the lock
        oasys::ScopeLock scoplok(bundle->lock(), __func__);

        SPtr_BlockInfoVec sptr_blocks = bundle->xmit_blocks()->find_blocks(contact_->link());

        if (sptr_blocks) {
            bool complete = false;
            u_char* buf;
            size_t max_buf_len = 2 * 1024 * 1024;
            size_t bytes_written = 0;
            int buf_size;
            int n;


            total_len = BundleProtocol::total_length(bundle, sptr_blocks.get());


            // TODO : Check length against the quota
            if (!sptr_esc_->disk_quota_is_available(total_len)) {
                // abort writing this bundle to file
                error_writing_file = true;
                break;
            }


            if (total_len < max_buf_len) {
                max_buf_len = total_len;
            }

            buf = (u_char*) malloc(max_buf_len);


            // loop writing buffers of data to the file
            while (bytes_written < total_len) {
                buf_size = BundleProtocol::produce(bundle, sptr_blocks.get(), buf, bytes_written, max_buf_len, &complete);

                if (buf_size > 0) {
                    // write this buffer out to the file
                    n = file_.write((const char*) buf, buf_size);

                    if (n <= 0) {
                        int save_errno = errno;

                        error_writing_file = true;

                        log_err("error writing to file: %s - %s", fullpath.c_str(), strerror(save_errno));

                        // declare an error state
                        if (save_errno == ENOSPC) {
                            sptr_esc_->set_cl_state(RESTAGE_CL_STATE_FULL);
                        } else {
                            sptr_esc_->set_cl_state(RESTAGE_CL_STATE_ERROR);
                        }

                        break;
                    } else {
                        ASSERT(n == buf_size);

                        bytes_written += buf_size;


                        if (complete) {
                            ASSERT(bytes_written == total_len);
                            break;
                        }
                    }

                } else {
                    if (!complete) {
                        error_writing_file = true;
                        log_err("produced zero bytes and complete is not set");
                    }
                    break;
                }
            }


            free(buf);
        } else {
            error_writing_file = true;
            log_err("Restage aborted - Unable to find xmit blocks for bundle: *%p", bundle);
        }
    } while (false);  // end of lock scope

    file_.close();

    if (error_writing_file) {
        // delete the [partial] file
        file_.unlink();

        return 0;
    } else {
        // get the disk usage for this file
        size_t disk_usage;

        struct stat stat_buf;
        if (0 == stat(fullpath.c_str(), &stat_buf)) {
            disk_usage = stat_buf.st_blocks * 512;
        } else {
            // fallback to actual file size
            disk_usage = total_len;
        }

        // let the Bundle Restaging Daemon know this bundle was restaged
        BundleDaemon::instance()->bundle_restaged(bundle, disk_usage);

        // let the ESC know this file was written to storage
        sptr_esc_->bundle_restaged(dirname, filename, bundle, total_len, disk_usage);

        return disk_usage;
    }
}

//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Reloader::Reloader(const std::string& link_name,
                                                                       SPtr_ExternalStorageController sptr_esc,
                                                                       SPtr_BundleArchitecturalRestagingDaemon sptr_bard)
    : Logger("RestageCL:ESC:Reloader",
             "/dtn/cl/%s/reloader", link_name.c_str()),
      Thread("RestageCL::ESC::Reloader"),
      link_name_(link_name),
      sptr_esc_(sptr_esc),
      sptr_bard_(sptr_bard),
      eventq_(logpath_)
{
    file_.logpathf("reload/file");
}

//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Reloader::~Reloader()
{
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Reloader::shutdown()
{
    set_should_stop();

    while (!is_stopped()) {
        usleep(100000);
    }

    // release the shared pointer to the parent ESC
    sptr_esc_.reset();
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Reloader::post(const std::string& dirname,
                                                                   SPtr_BundleFileDescMap sptr_bfd_map, 
                                                                   const std::string& dir_path, 
                                                                   size_t new_expiration, 
                                                                   const std::string&  new_dest_eid)
{
    if (should_stop()) {
        return;
    }

    ReloadEvent* event = new ReloadEvent();
    event->dirname_ = dirname;
    event->sptr_bfd_map_ = sptr_bfd_map;
    event->dir_path_ = dir_path;
    event->new_expiration_ = new_expiration;
    event->new_dest_eid_ = new_dest_eid;

    eventq_.push_back(event);
}



//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Reloader::run()
{
    char threadname[16] = "RstgReloader";
    pthread_setname_np(pthread_self(), threadname);

    ReloadEvent* event;

    while (!should_stop()) {
        if (paused_ || eventq_.size() == 0) {
            usleep(100000);
        } else {
            bool ok = eventq_.try_pop(&event);
            ASSERT(ok);
            
            if (event->new_expiration_ == -99) {
                process_delete_bundles(event);
            } else {
                process_reload(event);
            }

            delete event;
        }
    }
}

//----------------------------------------------------------------------
bool
RestageConvergenceLayer::ExternalStorageController::Reloader::query_okay_to_reload(const BundleFileDesc* bfd_ptr)
{
    // query the BARD to see if this bundle could be reloaded
    // - only check based on the EID that it was restaged by
    // - it is possible that it will be restaged again based on its other EID
    // - possibly on purpose to another location
    bard_quota_naming_schemes_t scheme;
    std::string nodename;

    bard_quota_type_t quota_type = bfd_ptr->quota_type_;

    if (quota_type == BARD_QUOTA_TYPE_SRC) {
        scheme = bfd_ptr->src_eid_scheme_;
        nodename = bfd_ptr->src_eid_nodename_;
    } else {
        scheme = bfd_ptr->dst_eid_scheme_;
        nodename = bfd_ptr->dst_eid_nodename_;
    }

    // Ask the Bundle Restaging Daemon
    return sptr_bard_->query_accept_reload_bundle(quota_type, scheme, nodename, bfd_ptr->payload_length_);
}



//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Reloader::process_reload(ReloadEvent* event)
{
    BundleDaemon* bdaemon = BundleDaemon::instance();
    SPtr_BundleFileDesc sptr_bfd;
    std::string fullpath;

    // make sure the dir path includes a trailing slash
    std::string dir_path = event->dir_path_ + "/";

    size_t  num_reloaded = 0;
    bool was_paused = false;

    BundleFileDescMapIter iter = event->sptr_bfd_map_->begin();


    while ((!should_stop()) && (iter != event->sptr_bfd_map_->end())) {
        while (!should_stop() && paused_) {
            was_paused = true;
            usleep(100000);
        }
        if (should_stop()) {
            break;
        }
        if (was_paused) {
            was_paused = false;
            // restart at the beginning of the list in case the garbage collector
            // deleted the file we were about to process
            iter = event->sptr_bfd_map_->begin();
            continue;
        }


        sptr_bfd = iter->second;

        bool reloaded = false;
        bool remove_not_found_file_from_list = false;

        if (!query_okay_to_reload(sptr_bfd.get())) {
            // not okay - we are done
            break;
        }

        fullpath = dir_path;
        fullpath.append(iter->first);

        // attempt to open the file

        int err = 0;
        int open_errno = 0;
        err = file_.open(fullpath.c_str(), O_RDONLY, &open_errno);
   
        if (err < 0)
        {
            if (open_errno == ENOENT) {
                log_err("Error #%zu on file for reload (open): %s : %s - removing from the file list", 
                        sptr_bfd->error_count_, fullpath.c_str(), strerror(open_errno));
                remove_not_found_file_from_list = true;

            } else {
                sptr_bfd->error_count_ += 1;

                log_err("Error #%zu on file for reload (open): %s : %s", 
                        sptr_bfd->error_count_, fullpath.c_str(), strerror(open_errno));

                // hopefully this is a one off - go on to the next file
                // what to do if errors continues??
            }
        } else {

            Bundle* bundle = extract_bundle_from_file(fullpath, sptr_bfd);

            file_.close();

            // 
            if (bundle != nullptr) {
                int64_t ttl_remaining = bundle->time_to_expiration_secs();

                // override the expiration time if needed
                if ((event->new_expiration_ > 0) && (ttl_remaining < (int64_t)event->new_expiration_)) {
                    uint64_t creation_time = bundle->creation_time_secs(); // Seconds since 1/1/2000
                    uint64_t now = BundleTimestamp::get_current_time_secs();
                    uint64_t new_exp = now - creation_time + event->new_expiration_;
                    bundle->set_expiration_secs(new_exp);
                }

                // override the destination EID if requested
                if (!event->new_dest_eid_.empty() > 0) {
                    bundle->mutable_dest()->assign(event->new_dest_eid_);

                    if (!bundle->dest().valid()) {
                        log_err("RestageCL(%s) : reload attempt with invalid override destination EID: %s", 
                                link_name_.c_str(), event->new_dest_eid_.c_str());
                        // abort any further processing
                        break;
                    }
                }


                // verify it is still okay to accept this bundle
                bool space_reserved = false;
                uint64_t prev_reserved_space = 0;
                bool accept_bundle = bdaemon->query_accept_bundle_based_on_quotas(bundle, space_reserved,
                                                                                  prev_reserved_space);

                if (accept_bundle) {
                    // check to make sure nothing changed and that the BARD is not
                    // going to restage it to this link
                    if (0 == link_name_.compare(bundle->bard_restage_link_name())) {
                        accept_bundle = false;
                    }
                }


                if (!accept_bundle) {
                    // There is no BundleRef object associated with this Bundle yet
                    // need to manually return any overall payload reserved space
                    // and also any BARD reserved space
                    bdaemon->release_bundle_without_bref_reserved_space(bundle);

                    delete bundle;

                } else {

                    bard_quota_naming_schemes_t scheme;
                    std::string nodename;
                    bard_quota_type_t quota_type = sptr_bfd->quota_type_;

                    if (quota_type == BARD_QUOTA_TYPE_DST) {
                        scheme = sptr_bfd->dst_eid_scheme_;
                        nodename = sptr_bfd->dst_eid_nodename_;
                    } else {
                        scheme = sptr_bfd->src_eid_scheme_;
                        nodename = sptr_bfd->src_eid_nodename_;
                    }

                    sptr_bard_->restaged_bundle_deleted(quota_type, scheme, nodename, sptr_bfd->disk_usage_);

                    // delete the file
                    file_.unlink();

                    
                    BundleDaemon::post( new BundleReceivedEvent(bundle, EVENTSRC_RESTAGE, 
                                                                sptr_bfd->file_size_, 
                                                                EndpointID::NULL_EID()));

                    reloaded = true;
                }
            }
        }

        ++iter;

        if (reloaded) {
            // done after incrementing to the next record because this one
            // will be deleted from the map

            sptr_esc_->bundle_reloaded(event->dirname_, event->sptr_bfd_map_.get(), sptr_bfd.get());

            ++num_reloaded;
        } else if (remove_not_found_file_from_list) {
            sptr_esc_->bundle_deleted(event->dirname_, event->sptr_bfd_map_.get(), sptr_bfd.get());
        }
    }


    log_debug("%s reloaded %zu bundles from %s", link_name_.c_str(), num_reloaded, event->dirname_.c_str());
}


//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Reloader::process_delete_bundles(ReloadEvent* event)
{
    SPtr_BundleFileDesc sptr_bfd;
    std::string fullpath;

    // make sure the dir path includes a trailing slash
    std::string dir_path = event->dir_path_ + "/";

    size_t num_deleted = 0;
    bool was_paused = false;

    BundleFileDescMapIter iter = event->sptr_bfd_map_->begin();


    while ((!should_stop()) && (iter != event->sptr_bfd_map_->end())) {
        while (!should_stop() && paused_) {
            was_paused = true;
            usleep(100000);
        }
        if (should_stop()) {
            break;
        }
        if (was_paused) {
            was_paused = false;
            // restart at the beginning of the list in case the garbage collector
            // deleted the file we were about to process
            iter = event->sptr_bfd_map_->begin();
            continue;
        }


        sptr_bfd = iter->second;

        fullpath = dir_path;
        fullpath.append(iter->first);

        // delete the file
        if (0 != unlink(fullpath.c_str())) {
            log_err("Error deleting restaged bundle file: %s - %s",
                    fullpath.c_str(), strerror(errno));
        }

        bard_quota_naming_schemes_t scheme;
        std::string nodename;
        bard_quota_type_t quota_type = sptr_bfd->quota_type_;

        if (quota_type == BARD_QUOTA_TYPE_DST) {
            scheme = sptr_bfd->dst_eid_scheme_;
            nodename = sptr_bfd->dst_eid_nodename_;
        } else {
            scheme = sptr_bfd->src_eid_scheme_;
            nodename = sptr_bfd->src_eid_nodename_;
        }

        sptr_bard_->restaged_bundle_deleted(quota_type, scheme, nodename, sptr_bfd->disk_usage_);


        ++iter;

        sptr_esc_->bundle_deleted(event->dirname_, event->sptr_bfd_map_.get(), sptr_bfd.get());
        ++num_deleted;
    }


    log_debug("%s deleted %zu bundles from %s", link_name_.c_str(), num_deleted, event->dirname_.c_str());
}


//----------------------------------------------------------------------
Bundle*
RestageConvergenceLayer::ExternalStorageController::Reloader::extract_bundle_from_file(const std::string& fullpath,
                                                                                       SPtr_BundleFileDesc sptr_bfd)
{
    Bundle* bundle = nullptr;


    // read the file in chunks and attempt to parse out the bundle

    size_t buffer_offset = 0;
    size_t bytes_to_read = sizeof(buffer_);
    ssize_t bytes_read = file_.read(buffer_, bytes_to_read);

   
    if (bytes_read <= 0) {
        sptr_bfd->error_count_ += 1;

        log_err("Error #%zu on file for reload (read): %s : %s", 
                sptr_bfd->error_count_, fullpath.c_str(), strerror(errno));

        return bundle;
    }


    bool abort = false;

    bundle = new Bundle(BundleProtocol::BP_VERSION_UNKNOWN);

    bool last;
    ssize_t cc;
    while (bytes_read > 0) {
        last = false;
        cc = BundleProtocol::consume(bundle, (u_char*)buffer_, bytes_read, &last);

        if (cc <= 0) {
            sptr_bfd->error_count_ += 1;

            log_err("Error #%zu on file for reload (consume): %s", 
                    sptr_bfd->error_count_, fullpath.c_str());

            abort = true;
            break;

        } else if (!last) {
            if (cc < bytes_read) {
                // not enough data at the end of the buffer to parse a bundle block
                // move the data to the front of the buffer and read some more data

                buffer_offset = cc;
                bytes_to_read = sizeof(buffer_) - cc;
                memmove(buffer_, &buffer_[buffer_offset], bytes_to_read);
                
            } else {
                // read another full chunk fr proessing
                buffer_offset = 0;
                bytes_to_read = sizeof(buffer_);
            }

            // read another chunk of data
            bytes_read = file_.read(&buffer_[buffer_offset], bytes_to_read);

            if (bytes_read <= 0) {
                sptr_bfd->error_count_ += 1;

                log_err("Error #%zu on file for reload (read): %s : %s", 
                        sptr_bfd->error_count_, fullpath.c_str(), strerror(errno));

                abort = true;
                break;
            }

            // adjust bytes_read to the full amount of data available in the buffer
            bytes_read += buffer_offset;

        } else {
            // extracted a full bundle

            if (cc != bytes_read) {
                log_err("extracted bundle but extra data detected in restaged bundle file: %s",
                        fullpath.c_str());

                // got our bundle so we shall go with it
            }

            break;
        }
    }

    if (abort) {
        delete bundle;
        bundle = nullptr;
    }

    return bundle;
}




//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Emailer::Emailer(restage_cl_states_t new_state, 
                                                                     SPtr_EmailNotifications sptr_emails)
    : Logger("RestageCL:ESC:Emailer",
             "/dtn/cl/restage/emailer"),
      Thread("RestageCL::ESC::Emailer", Thread::DELETE_ON_EXIT),
      new_state_(new_state),
      sptr_emails_(sptr_emails)
{
}

//----------------------------------------------------------------------
RestageConvergenceLayer::ExternalStorageController::Emailer::~Emailer()
{
}

//----------------------------------------------------------------------
void
RestageConvergenceLayer::ExternalStorageController::Emailer::run()
{
    char threadname[16] = "RstgEmailer";
    pthread_setname_np(pthread_self(), threadname);

    std::string email_addr;

    oasys::ScopeLock scoplok(&sptr_emails_->lock_, __func__);

    EmailListIter iter = sptr_emails_->email_list_.begin();
    while (iter != sptr_emails_->email_list_.end()) {
        email_addr = iter->first;

        log_always("Send email from: %s  to: %s  - new state = %s",
                   sptr_emails_->from_email_.c_str(),
                   email_addr.c_str(), 
                   restage_cl_states_to_str(new_state_));


        // plagierized from stackoverflow.com #9317305
        FILE* mailpipe = popen("/usr/lib/sendmail -t", "w");

        if (mailpipe != nullptr) {
            fprintf(mailpipe, "To: %s\n", email_addr.c_str());
            fprintf(mailpipe, "From: %s\n", sptr_emails_->from_email_.c_str());
            fprintf(mailpipe, "Subject: DTNME Restage CL State Change Alert - %s\n\n", 
                    restage_cl_states_to_str(new_state_));
            fprintf(mailpipe, "DTNME Restage CL changed to state: %s\n",
                    restage_cl_states_to_str(new_state_));
            fprintf(mailpipe, ".\n");
            pclose(mailpipe);
        } else {
            log_err("Error invoking sendmail for DTNME Restage CL State Change to %s alert",
                    restage_cl_states_to_str(new_state_));
        }

        ++iter;
    }
    
}

} // namespace dtn

#endif // BARD_ENABLED

