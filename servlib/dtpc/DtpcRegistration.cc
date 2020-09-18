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

#include "DtpcRegistration.h"

namespace dtn {

//----------------------------------------------------------------------
DtpcRegistration::DtpcRegistration(u_int32_t topic_id, bool has_elision_func)
    : APIRegistration(topic_id, EndpointID::NULL_EID(), Registration::DROP,
                      Registration::NONE, 0, 0, false),
      has_elision_func_(has_elision_func)
{
    collector_list_ = new BlockingDtpcTopicCollectorList("regcols");
    aggregator_list_ = new BlockingDtpcTopicAggregatorList("regaggs");
}


//----------------------------------------------------------------------
DtpcRegistration::DtpcRegistration(const oasys::Builder& bldr)
    : APIRegistration(bldr),
      has_elision_func_(false)
{
    collector_list_ = new BlockingDtpcTopicCollectorList("regcols");
    aggregator_list_ = new BlockingDtpcTopicAggregatorList("regaggs");
}

//----------------------------------------------------------------------
DtpcRegistration::~DtpcRegistration () 
{
    delete collector_list_;
    delete aggregator_list_;
}


//----------------------------------------------------------------------
void
DtpcRegistration::serialize(oasys::SerializeAction* a)
{
    (void) a;
    //a->process("topic_id", &topic_id_);

    //XXX/dz TODO:  list of data PDUs...
}

//----------------------------------------------------------------------
void 
DtpcRegistration::deliver_topic_collector(DtpcTopicCollector* collector)
{
    collector_list_->push_back(collector);
}

//----------------------------------------------------------------------
void 
DtpcRegistration::add_topic_aggregator(DtpcTopicAggregator* aggregator)
{
    aggregator_list_->push_back(aggregator);
}

} // namespace dtn

#endif // DTPC_ENABLED 
