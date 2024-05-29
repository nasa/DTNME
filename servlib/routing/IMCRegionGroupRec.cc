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

#include "IMCRegionGroupRec.h"

namespace dtn {

//----------------------------------------------------------------------
IMCRegionGroupRec::IMCRegionGroupRec(ssize_t rec_type) 
    : rec_type_(rec_type)
{}


//----------------------------------------------------------------------
IMCRegionGroupRec::IMCRegionGroupRec(const oasys::Builder&)
{}

//----------------------------------------------------------------------
IMCRegionGroupRec::~IMCRegionGroupRec () {
}


//----------------------------------------------------------------------
void
IMCRegionGroupRec::serialize(oasys::SerializeAction* a)
{
    if (a->action_code() != oasys::Serialize::UNMARSHAL) {
        set_key_string();
    }

    a->process("key", &key_);
    a->process("rec_type", &rec_type_);
    a->process("reg_or_grp", &region_or_group_num_);
    a->process("node_or_id_num", &node_or_id_num_);
    a->process("operation", &operation_);
    a->process("is_router_node", &is_router_node_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        set_key_string();
    }
}

//----------------------------------------------------------------------
void
IMCRegionGroupRec::set_key_string()
{
    switch (rec_type_) {
        case INC_REC_TYPE_HOME_REGION:
            key_ = "home_region";
            break;

        case IMC_REC_TYPE_REGION_DB_CLEAR:
            key_ = "clear_region_db";
            break;

        case IMC_REC_TYPE_GROUP_DB_CLEAR:
            key_ = "clear_group_db";
            break;

        default:
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "%zd_%zu_%zu",
                     rec_type_, region_or_group_num_, node_or_id_num_);
            key_ = tmp;
    }
}

//----------------------------------------------------------------------
std::string
IMCRegionGroupRec::durable_key()
{
    set_key_string();

    return key_;
}

} // namespace dtn

