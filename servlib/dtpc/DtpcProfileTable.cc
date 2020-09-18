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

#include "oasys/util/OptParser.h"
#include "oasys/util/StringBuffer.h"

#include "DtpcProfileStore.h"
#include "DtpcProfileTable.h"

namespace dtn {

DtpcProfileTable* DtpcProfileTable::instance_ = NULL;



//----------------------------------------------------------------------
DtpcProfileTable::DtpcProfileTable()
    : Logger("DtpcProfileTable", "/dtpc/profile/table"),
      storage_initialized_(false)
{
}

//----------------------------------------------------------------------
DtpcProfileTable::~DtpcProfileTable()
{
    oasys::ScopeLock l(&lock_, "DtpcProfileTable destructor");

    DtpcProfileIterator iter;
    for(iter = profile_list_.begin();iter != profile_list_.end(); iter++) {
        delete iter->second;
    }

}

//----------------------------------------------------------------------
void 
DtpcProfileTable::shutdown() {
    delete instance_;
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::find(const u_int32_t profile_id)
{
    DtpcProfileIterator iter;

    oasys::ScopeLock l(&lock_, "find (public)");

    iter = profile_list_.find(profile_id);
    return (profile_list_.end() != iter);
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::find(const u_int32_t profile_id,
                       DtpcProfileIterator* iter)
{
    // lock obtained by caller
    *iter = profile_list_.find(profile_id);
    return (profile_list_.end() != *iter);
}

//----------------------------------------------------------------------
DtpcProfile* 
DtpcProfileTable::get(const u_int32_t profile_id)
{
    oasys::ScopeLock l(&lock_, "get");

    DtpcProfileIterator iter;
    if (! find(profile_id, &iter)) {
        return NULL;
    } else {
        return iter->second;
    }
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::parse_params(DtpcProfile* profile,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    std::string replyto;
    std::string priority;
    bool replyto_set = false;
    bool priority_set = false;

    oasys::OptParser p;

    p.addopt(new oasys::IntOpt("retran", &profile->retransmission_limit_));
    p.addopt(new oasys::UIntOpt("agg_size", &profile->aggregation_size_limit_));
    p.addopt(new oasys::UIntOpt("agg_time", &profile->aggregation_time_limit_));
    p.addopt(new oasys::BoolOpt("custody", &profile->custody_transfer_));
    p.addopt(new oasys::UInt64Opt("lifetime", &profile->expiration_));
    p.addopt(new oasys::StringOpt("replyto", &replyto, "", "", &replyto_set));
    p.addopt(new oasys::StringOpt("priority", &priority, "", "", &priority_set));
    p.addopt(new oasys::UInt8Opt("ecos", &profile->ecos_ordinal_));
    p.addopt(new oasys::BoolOpt("rpt_rcpt", &profile->rpt_reception_));
    p.addopt(new oasys::BoolOpt("rpt_acpt", &profile->rpt_acceptance_));
    p.addopt(new oasys::BoolOpt("rpt_fwrd", &profile->rpt_forward_));
    p.addopt(new oasys::BoolOpt("rpt_dlvr", &profile->rpt_delivery_));
    p.addopt(new oasys::BoolOpt("rpt_dele", &profile->rpt_deletion_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    // finish parsing the ReplyTo EID if provided
    if (replyto_set) {
        EndpointID eid(replyto);
        if (!eid.valid() || !eid.known_scheme()) {
            // point to the invalid parameter
            if (invalidp) {
                for (int ix=0; ix<argc; ++ix) {
                    if (0 == strncmp("replyto", argv[ix], strlen("replyto"))) {
                        *invalidp = argv[ix];
                    }
                }
            } 
            return false;
        }

        // store the replyto EID in theprofile
        profile->mutable_replyto()->assign(replyto);
    }

    // finish parsing the priority if provided
    if (priority_set) {
        if ((0 == priority.compare("b")) ||
            (0 == priority.compare("bulk")) ||
            (0 == priority.compare("0"))) {
            profile->set_priority(0);   // TODO: enum type
        }
        else if ((0 == priority.compare("n")) ||
                 (0 == priority.compare("normal")) ||
                 (0 == priority.compare("1"))) {
            profile->set_priority(1);   // TODO: enum type
        }
        else if ((0 == priority.compare("e")) ||
                 (0 == priority.compare("expedited")) ||
                 (0 == priority.compare("2"))) {
            profile->set_priority(2);   // TODO: enum type
        }
        else if ((0 == priority.compare("r")) ||
                 (0 == priority.compare("reserved")) ||
                 (0 == priority.compare("3"))) {
            profile->set_priority(3);   // TODO: enum type
        }
        else {
            // point to the invalid parameter
            if (invalidp) {
                for (int ix=0; ix<argc; ++ix) {
                    if (0 == strncmp("priority", argv[ix], strlen("priority"))) {
                        *invalidp = argv[ix];
                    }
                }
            } 
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::add(oasys::StringBuffer *errmsg, const u_int32_t profile_id, 
                      int argc, const char* argv[])
{
    DtpcProfileIterator iter;
    
    oasys::ScopeLock l(&lock_, "add");

    if (find(profile_id, &iter)) {
        log_err("attempt to add Profile ID %"PRIu32" which already exists", profile_id);
        if (errmsg) {
            errmsg->appendf("already exists");
        }
        return false;
    }
    
    DtpcProfile* profile = new DtpcProfile(profile_id);

    const char* invalidp;
    if (!parse_params(profile, argc, argv, &invalidp)) {
        log_err("attempt to add Profile ID %"PRIu32" with invalid parameter: %s", profile_id, invalidp);
        if (errmsg) {
            errmsg->appendf("invalid parameter: %s", invalidp);
        }
        delete profile;
        return false;
    }

    if ((0 == profile->aggregation_size_limit() && 0 != profile->aggregation_time_limit()) ||
        (0 != profile->aggregation_size_limit() && 0 == profile->aggregation_time_limit()))
    {
        log_err("attempt to add Profile ID %"PRIu32" with invalid aggregation parameters: size: %d time: %d", 
                profile_id, profile->aggregation_size_limit(), profile->aggregation_time_limit());
        if (errmsg) {
            errmsg->appendf("invalid aggragation parameters - both must be zero or non-zero ");
        }
        delete profile;
        return false;
    }

    log_info("adding DTPC transmission profile %"PRIu32, profile_id);

    profile_list_.insert(DtpcProfilePair(profile_id, profile));

    if (storage_initialized_) {
        log_debug("adding profile %"PRIu32" to ProfileTable", profile_id);
        profile->set_queued_for_datastore(true);
        DtpcProfileStore::instance()->add(profile);
        profile->set_in_datastore(true);
    }

    return true;
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::add_reloaded_profile(DtpcProfile* profile)
{
    u_int32_t profile_id = profile->profile_id();

    DtpcProfileIterator iter;
    
    oasys::ScopeLock l(&lock_, "add_reloaded_profile");

    if (find(profile_id, &iter)) {
        DtpcProfile* found_profile = iter->second;
        if (*found_profile != *profile) {
            oasys::StringBuffer buf;
            buf.appendf("Reloaded profile (%"PRIu32") does not match definition in configuration file (using config):\n", 
                     profile->profile_id());
            buf.appendf("           ProfID Retran AggSize AggTime Cust Lifetime Priority  ECOS Rcpt Acpt Fwrd Dlvr Dele ReplyTo\n");
            buf.appendf("           ------ ------ ------- ------- ---- -------- --------- ---- ---- ---- ---- ---- ---- --------------------\n");
            buf.appendf("DataStore: ");
            profile->format_for_list(&buf);
            buf.appendf("ConfgFile: ");
            found_profile->format_for_list(&buf);
            log_multiline(oasys::LOG_WARN, buf.c_str());
        } else {
            log_debug("Reloaded profile (%"PRIu32") matches definition in configuration file", 
                     profile->profile_id());
        }
        found_profile->set_reloaded_from_ds();

        // keep the configuration version and delete the reloaded one
        delete profile;

        return false;
    }
    
    log_info("adding reloaded DTPC transmission profile %"PRIu32, profile_id);

    profile_list_.insert(DtpcProfilePair(profile_id, profile));

    return true;
}

//----------------------------------------------------------------------
bool
DtpcProfileTable::del(const u_int32_t profile_id)
{
    DtpcProfileIterator iter;
    DtpcProfile* profile;
    
    log_info("removing DTPC transmission profile %"PRIu32, profile_id);

    oasys::ScopeLock l(&lock_, "del");

    if (! find(profile_id, &iter)) {
        log_err("error removing profile %"PRIu32": not in DtpcProfileTable",
                profile_id);
        return false;
    }

    profile = iter->second;
    profile_list_.erase(iter);

    if (storage_initialized_) {
        DtpcProfileStore::instance()->del(profile_id);
    }

    delete profile;
    return true;
}

//----------------------------------------------------------------------
void
DtpcProfileTable::list(oasys::StringBuffer *buf)
{
    DtpcProfileIterator iter;
    DtpcProfile* profile;

    oasys::ScopeLock l(&lock_, "list");

    if (1 == profile_list_.size())
        buf->appendf("Transmission Profile Table (1 entry):\n");
    else
      buf->appendf("Transmission Profile Table (%zu entries):\n", profile_list_.size());

    buf->appendf("ProfID Retran AggSize AggTime Cust Lifetime Priority  ECOS Rcpt Acpt Fwrd Dlvr Dele ReplyTo\n");
    buf->appendf("------ ------ ------- ------- ---- -------- --------- ---- ---- ---- ---- ---- ---- --------------------\n");

    for (iter = profile_list_.begin(); iter != profile_list_.end(); ++(iter)) {
        profile = iter->second;
        profile->format_for_list(buf);
    }
}

//----------------------------------------------------------------------
void
DtpcProfileTable::storage_initialized()
{
    storage_initialized_ = true;

    // XXX/dz loop through and add any new profiles to storage
    DtpcProfileIterator iter;
    DtpcProfile* profile;

    oasys::ScopeLock l(&lock_, "storage_initialized");

    for (iter = profile_list_.begin(); iter != profile_list_.end(); ++(iter)) {
        profile = iter->second;
        if (!profile->queued_for_datastore()) {
            log_info("adding DTPC transmission profile %"PRIu32" to ProfileStore", 
                     profile->profile_id());
            profile->set_queued_for_datastore(true);
            DtpcProfileStore::instance()->add(profile);
            profile->set_in_datastore(true);
        }
    }
}

} // namespace dtn

#endif // DTPC_ENABLED
