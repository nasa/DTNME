/*
 *    Copyright 2022 United States Government as represented by NASA
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

#include <third_party/oasys/util/ScratchBuffer.h>

#include "IMCGroupPetitionRegistration.h"
#include "routing/IMCRouter.h"

namespace dtn {

IMCGroupPetitionRegistration::IMCGroupPetitionRegistration(const SPtr_EID& sptr_eid)
    : Registration(IMC_GROUP_PETITION_REGID, sptr_eid, Registration::DEFER, Registration::NEW, 0, 0) 
{
    logpathf("/dtn/reg/imcgrppetition");
    set_active(true);

    reg_list_type_str_ = "ImcGroupPetitionReg";
}

int
IMCGroupPetitionRegistration::deliver_bundle(Bundle* bundle, SPtr_Registration& sptr_reg)
{
    if (bundle->is_bpv6()) {
        return deliver_bundle_bp6(bundle, sptr_reg);
    } else if (bundle->is_bpv7()) {
        return deliver_bundle_bp7(bundle, sptr_reg);
    } else {
        return REG_DELIVER_BUNDLE_DROPPED;
    }
}

int
IMCGroupPetitionRegistration::deliver_bundle_bp6(Bundle* bundle, SPtr_Registration& sptr_reg)
{
    (void) bundle;
    (void) sptr_reg;

    return REG_DELIVER_BUNDLE_DROPPED;
}

int
IMCGroupPetitionRegistration::deliver_bundle_bp7(Bundle* bundle, SPtr_Registration& sptr_reg)
{
    (void) sptr_reg;

    if (IMCRouter::imc_config_.process_imc_group_petition(bundle)) {
        //log_always("Success processing Group Petition bundle: *%p", bundle);
    } else {
        log_err("Error processing Group Petition bundle, bundlei: *%p", bundle);
    }
    return REG_DELIVER_BUNDLE_SUCCESS;
}

} // namespace dtn
