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

#include <stdint.h>
#include <limits.h>

#include "DtpcProfile.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcProfile::DtpcProfile(u_int32_t profile_id)
    : profile_id_(profile_id),
      custody_transfer_(false),
      expiration_(864000),
      replyto_("dtn:none"),
      priority_(0),
      ecos_ordinal_(0),
      rpt_reception_(false),
      rpt_acceptance_(false),
      rpt_forward_(false),
      rpt_delivery_(false),
      rpt_deletion_(false),
      retransmission_limit_(0),
      aggregation_size_limit_(1000),
      aggregation_time_limit_(60),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false)
{}


//----------------------------------------------------------------------
DtpcProfile::DtpcProfile(const oasys::Builder&)
    : profile_id_(0),
      custody_transfer_(false),
      expiration_(864000),
      replyto_("dtn:none"),
      priority_(0),
      ecos_ordinal_(0),
      rpt_reception_(false),
      rpt_acceptance_(false),
      rpt_forward_(false),
      rpt_delivery_(false),
      rpt_deletion_(false),
      retransmission_limit_(0),
      aggregation_size_limit_(1000),
      aggregation_time_limit_(60),
      in_datastore_(false),
      queued_for_datastore_(false),
      reloaded_from_ds_(false)
{}

//----------------------------------------------------------------------
DtpcProfile::~DtpcProfile () 
{}


//----------------------------------------------------------------------
void
DtpcProfile::serialize(oasys::SerializeAction* a)
{
    a->process("profile_id", &profile_id_);
    a->process("custody_transfer", &custody_transfer_);
    a->process("replyto", &replyto_);
    a->process("expiration", &expiration_);
    a->process("priority", &priority_);
    a->process("ecos_ordinal", &ecos_ordinal_);
    a->process("rpt_reception", &rpt_reception_);
    a->process("rpt_acceptance", &rpt_acceptance_);
    a->process("rpt_forward", &rpt_forward_);
    a->process("rpt_delivery", &rpt_delivery_);
    a->process("rpt_deletion", &rpt_deletion_);
    a->process("retransmission_limit", &retransmission_limit_);
    a->process("aggregation_size_limit", &aggregation_size_limit_);
    a->process("aggregation_time_limit", &aggregation_time_limit_);
}

//----------------------------------------------------------------------
const char*
DtpcProfile::priority_str()
{
    switch (priority_) {
        case 0:	 return "BULK";
        case 1:	 return "NORMAL";
        case 2:	 return "EXPEDITED";
        case 3:	 return "RESERVED";
        default: return "UNKNOWN";
    } 
}

//----------------------------------------------------------------------
void
DtpcProfile::format_for_list(oasys::StringBuffer* buf)
{
#define bool_to_c(x)   ((x) ? 'T' : 'F')

    buf->appendf("%6"PRIu32" %6"PRIu32" %7"PRIu32" %7"PRIu32"  %c   %8"PRIu64" %-9.9s  %3u  %c    %c    %c    %c    %c   %s\n",
                 profile_id_, retransmission_limit_, aggregation_size_limit_, aggregation_time_limit_,
                 bool_to_c(custody_transfer_), expiration_, priority_str(),
                 ecos_ordinal_, bool_to_c(rpt_reception_), bool_to_c(rpt_acceptance_),
                 bool_to_c(rpt_forward_), bool_to_c(rpt_delivery_), bool_to_c(rpt_deletion_),
                 replyto_.c_str());
}

//----------------------------------------------------------------------
void
DtpcProfile::format_verbose(oasys::StringBuffer* buf)
{

#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("profile id %"PRIu32":\n", profile_id_);
    buf->appendf("              dest: %s\n", replyto_.c_str());
}

//----------------------------------------------------------------------
struct timeval
DtpcProfile::calc_retransmit_time()
{
    struct timeval retran;
    gettimeofday(&retran, 0);
    retran.tv_sec += (expiration_ / (retransmission_limit_ + 1));
    return retran;
}


//----------------------------------------------------------------------
void 
DtpcProfile::set_reloaded_from_ds()
{ 
    reloaded_from_ds_ = true;
    set_in_datastore(true);
    set_queued_for_datastore(true);
}

//----------------------------------------------------------------------
bool 
DtpcProfile::operator==(const DtpcProfile& other) const
{
    return (profile_id_ == other.profile_id_)
        && (custody_transfer_ == other.custody_transfer_)
        && (replyto_ == other.replyto_)
        && (expiration_ == other.expiration_)
        && (priority_ == other.priority_)
        && (ecos_ordinal_ == other.ecos_ordinal_)
        && (rpt_reception_ == other.rpt_reception_)
        && (rpt_acceptance_ == other.rpt_acceptance_)
        && (rpt_forward_ == other.rpt_forward_)
        && (rpt_delivery_ == other.rpt_delivery_)
        && (rpt_deletion_ == other.rpt_deletion_)
        && (retransmission_limit_ == other.retransmission_limit_)
        && (aggregation_size_limit_ == other.aggregation_size_limit_)
        && (aggregation_time_limit_ == other.aggregation_time_limit_);
}


} // namespace dtn

#endif // DTPC_ENABLED
