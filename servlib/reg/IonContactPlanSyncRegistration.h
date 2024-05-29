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

#ifndef _ION_CONTACT_PLAN_SYNC_H_
#define _ION_CONTACT_PLAN_SYNC_H_

#include "Registration.h"

namespace dtn {

/**
 * Internal registration that recieves all ION COntact Plan Sync Bundles
 * (dest EID = imc:0.1)
*/
class IonContactPlanSyncRegistration : public Registration {
public:
    IonContactPlanSyncRegistration(const SPtr_EID& sptr_eid);

    /**
     * Deliver the given bundle.
     */
    int deliver_bundle(Bundle* bundle, SPtr_Registration& sptr_reg) override;

protected:
    int deliver_bundle_bp6(Bundle* bundle, SPtr_Registration& sptr_reg);
    int deliver_bundle_bp7(Bundle* bundle, SPtr_Registration& sptr_reg);

    const char* fld_name() { return fld_name_.c_str(); }

private:
    std::string fld_name_;

};

} // namespace dtn

#endif /* _ION_CONTACT_PLAN_SYNC_H_ */
