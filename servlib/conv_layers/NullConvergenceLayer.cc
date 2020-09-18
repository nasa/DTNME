/*
 *    Copyright 2006 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/OptParser.h>
#include "NullConvergenceLayer.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

class NullConvergenceLayer::Params NullConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
NullConvergenceLayer::Params::serialize(oasys::SerializeAction* a)
{
    a->process("can_transmit", &can_transmit_);
}

//----------------------------------------------------------------------
NullConvergenceLayer::NullConvergenceLayer()
  : ConvergenceLayer("NullConvergenceLayer", "null")
{
    defaults_.can_transmit_ = true;
}

//----------------------------------------------------------------------
CLInfo*
NullConvergenceLayer::new_link_params()
{
    return new NullConvergenceLayer::Params(NullConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
NullConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&NullConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
NullConvergenceLayer::parse_link_params(Params* params,
                                        int argc, const char** argv,
                                        const char** invalidp)
{
    oasys::OptParser p;
    p.addopt(new oasys::BoolOpt("can_transmit", &params->can_transmit_));
    return p.parse(argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
NullConvergenceLayer::init_link(const LinkRef& link,
                                int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);

    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }
    link->set_cl_info(params);
    return true;
}

//----------------------------------------------------------------------
bool
NullConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != NULL);
    
    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
void
NullConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != NULL);

    buf->appendf("can_transmit: %s\n", params->can_transmit_ ? "yes" : "no");
}

//----------------------------------------------------------------------
void
NullConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("deleting link %s", link->name());
    
    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
NullConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

//----------------------------------------------------------------------
void
NullConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    Params* params = (Params*)link->cl_info();

    if (! params->can_transmit_) {
        return;
    }
    
    const BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(link);
    ASSERT(blocks != NULL);
    size_t total_len = BundleProtocol::total_length(blocks);
    
    log_debug("send_bundle *%p to *%p (total len %zu)",
              bundle.object(), link.object(), total_len);
    
    link->del_from_queue(bundle, total_len);
    link->add_to_inflight(bundle, total_len);
    
    BundleDaemon::post(
        new BundleTransmittedEvent(bundle.object(), link->contact(), link, total_len, 0));
}

//----------------------------------------------------------------------
void
NullConvergenceLayer::cancel_bundle(const LinkRef& link, const BundleRef& bundle)
{
    Params* params = (Params*)link->cl_info();
    
    // if configured to not sent bundles, and if the bundle in
    // question is still on the link queue, then it can be cancelled
    if (! params->can_transmit_&& link->queue()->contains(bundle)) {
        log_debug("NullConvergenceLayer::cancel_bundle: "
                  "cancelling bundle *%p on *%p", bundle.object(), link.object());
        BundleDaemon::post(new BundleSendCancelledEvent(bundle.object(), link));
        return;
    } else {
        log_debug("NullConvergenceLayer::cancel_bundle: "
                  "not cancelling bundle *%p on *%p since !is_queued()",
                  bundle.object(), link.object());
    }
}

} // namespace dtn
