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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include <math.h>

#include <oasys/util/Options.h>
#include <oasys/util/OptParser.h>

#include "TrAgent.h"
#include "Simulator.h"
#include "Node.h"
#include "SimEvent.h"
#include "SimLog.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"

namespace dtnsim {

//----------------------------------------------------------------------
TrAgent::TrAgent(const EndpointID& src, const EndpointID& dst)
    : Logger("TrAgent", "/sim/tragent/%s", Node::active_node()->name()),
      src_(src), dst_(dst),
      size_(0), expiration_(30), reps_(0), batch_(1), interval_(0)
{
}

//----------------------------------------------------------------------
TrAgent*
TrAgent::init(const EndpointID& src, const EndpointID& dst,
              int argc, const char** argv)
{
    TrAgent* a = new TrAgent(src, dst);

    oasys::OptParser p;
    p.addopt(new oasys::SizeOpt("size", &a->size_));
    p.addopt(new oasys::UIntOpt("expiration", &a->expiration_));
    p.addopt(new oasys::UIntOpt("reps", &a->reps_));
    p.addopt(new oasys::UIntOpt("batch", &a->batch_));
    p.addopt(new oasys::DoubleOpt("interval", &a->interval_));

    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        a->logf(oasys::LOG_ERR, "invalid option: %s", invalid);
        return NULL;
    }

    if (a->size_ == 0) {
        a->logf(oasys::LOG_ERR, "size must be set in configuration");
        return NULL;
    }

    if (a->reps_ == 0) {
        a->logf(oasys::LOG_ERR, "reps must be set in configuration");
        return NULL;
    }

    if (a->reps_ != 1 && a->interval_ == 0) {
        a->logf(oasys::LOG_ERR, "interval must be set in configuration");
        return NULL;
    }

    a->schedule_immediate();
    return a;
}

//----------------------------------------------------------------------
void
TrAgent::timeout(const struct timeval& /* now */)
{
    for (u_int i = 0; i < batch_; i++) {
        send_bundle();
    }
        
    if (--reps_ > 0) {
        log_debug("scheduling timer in %u ms", (u_int)(interval_ * 1000));
        schedule_in((int)(interval_ * 1000));
    } else {
        log_debug("all batches finished");
    }
}

//----------------------------------------------------------------------
void
TrAgent::send_bundle()
{
    Bundle* b = new Bundle(BundlePayload::NODATA);
        
    //oasys::StaticStringBuffer<1024> buf;
    //b->format_verbose(&buf);
    //log_multiline(oasys::LOG_DEBUG, buf.c_str());
        
    b->mutable_source()->assign(src_);
    b->mutable_replyto()->assign(src_);
    b->mutable_custodian()->assign(EndpointID::NULL_EID());
    b->mutable_dest()->assign(dst_);
    b->mutable_payload()->set_length(size_);
        
    b->set_priority(0);
    b->set_custody_requested(false);
    b->set_local_custody(false);
    b->set_singleton_dest(false);
    b->set_receive_rcpt(false);
    b->set_custody_rcpt(false);
    b->set_forward_rcpt(false);
    b->set_delivery_rcpt(false);
    b->set_deletion_rcpt(false);
    b->set_app_acked_rcpt(false);
    b->set_creation_ts(BundleTimestamp(BundleTimestamp::get_current_time(),
                                       b->bundleid()));
    b->set_expiration(expiration_);
    b->set_is_fragment(false);
    b->set_is_admin(false);
    b->set_do_not_fragment(false);
    b->set_in_datastore(false);
    //b->orig_length_   = 0;
    //b->frag_offset_   = 0;    
    
    log_info("N[%s]: GEN id:%" PRIbid " %s -> %s size:%llu",
             Node::active_node()->name(), b->bundleid(),
             src_.c_str(), dst_.c_str(), U64FMT(size_));

    SimLog::instance()->log_gen(Node::active_node(), b);
		
    BundleDaemon::post(new BundleReceivedEvent(b, EVENTSRC_APP,
                                               NULL /* registration? */));
}


} // namespace dtnsim
