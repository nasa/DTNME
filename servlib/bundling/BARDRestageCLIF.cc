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
        case(RESTAGE_CL_STATE_ONLINE): return "online";
        case(RESTAGE_CL_STATE_FULL): return "full";
        case(RESTAGE_CL_STATE_ERROR): return "error";
        case(RESTAGE_CL_STATE_DELETED): return "deleted";
        default:
            return "unknown";
    }
}

restage_cl_states_t str_to_restage_cl_state(const char* state)
{
    if (0 == strcasecmp(state, "online")) {
        return RESTAGE_CL_STATE_ONLINE;
    } else if (0 == strcasecmp(state, "full")) {
        return RESTAGE_CL_STATE_FULL;
    } else if (0 == strcasecmp(state, "error")) {
        return RESTAGE_CL_STATE_ERROR;
    } else if (0 == strcasecmp(state, "deleted")) {
        return RESTAGE_CL_STATE_DELETED;
   } else {
        return RESTAGE_CL_STATE_UNDEFINED;;
   }
};


}    

#endif // BARD_ENABLED

