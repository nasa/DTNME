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

#include "ConvergenceLayer.h"
#include "BIBEConvergenceLayer.h"
#include "NullConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "TCPConvergenceLayerV3.h"
#include "UDPConvergenceLayer.h"

#include "LTPUDPConvergenceLayer.h"
#include "LTPUDPReplayConvergenceLayer.h"

#include "STCPConvergenceLayer.h"
#include "MinimalTCPConvergenceLayer.h"

#include "RestageConvergenceLayer.h"

#include "bundling/BundleDaemon.h"

namespace oasys {
    template<> dtn::CLVector* oasys::Singleton<dtn::CLVector>::instance_ = NULL;
}

namespace dtn {

//----------------------------------------------------------------------
ConvergenceLayer::~ConvergenceLayer()
{
}

//----------------------------------------------------------------------
void
ConvergenceLayer::add_clayer(ConvergenceLayer* cl)
{
    CLVector::instance()->push_back(cl);
}
    
//----------------------------------------------------------------------
void
ConvergenceLayer::init_clayers()
{
    add_clayer(new NullConvergenceLayer());
    add_clayer(new TCPConvergenceLayer());
    add_clayer(new TCPConvergenceLayerV3());
    add_clayer(new UDPConvergenceLayer());

    add_clayer(new LTPUDPConvergenceLayer());
    add_clayer(new LTPUDPReplayConvergenceLayer());


    add_clayer(new STCPConvergenceLayer());
    add_clayer(new MinimalTCPConvergenceLayer());

    add_clayer(new BIBEConvergenceLayer());

#if BARD_ENABLED
    add_clayer(new RestageConvergenceLayer());
#endif // BARD_ENABLED

}
//----------------------------------------------------------------------
CLVector::~CLVector()
{
    while (!empty()) {
        delete back();
        pop_back();
    }
}

//----------------------------------------------------------------------
ConvergenceLayer*
ConvergenceLayer::find_clayer(const char* name)
{
    CLVector::iterator iter;
    for (iter = CLVector::instance()->begin();
         iter != CLVector::instance()->end();
         ++iter)
    {
        if (strcasecmp(name, (*iter)->name()) == 0) {
            return *iter;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::shutdown_clayers()
{
    CLVector::iterator iter;
    for (iter = CLVector::instance()->begin();
         iter != CLVector::instance()->end();
         ++iter)
    {
        (*iter)->shutdown();

        //dzdebug
        delete (*iter);
    }

    CLVector::instance()->clear();
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_cla_parameters(AttributeVector &params)
{
    (void)params;
    log_debug("set cla parameters");
    // probably only used by the external convergence layer???
    CLAParamsSetEvent* event_to_post;
    event_to_post = new CLAParamsSetEvent(this, "");
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
    return true;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_interface_defaults(int argc, const char* argv[],
                                         const char** invalidp)
{
    if (argc == 0) {
        return true;
    } else {
        *invalidp = argv[0];
        return false;
    }
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                    const char** invalidp)
{
    if (argc == 0) {
        return true;
    } else {
        *invalidp = argv[0];
        return false;
    }
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::interface_up(Interface* iface,
                               int argc, const char* argv[])
{
    (void)iface;
    (void)argc;
    (void)argv;
    log_debug("init interface %s", iface->name().c_str());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::interface_activate(Interface* iface)
{
    (void)iface;
    log_debug("activate interface %s", iface->name().c_str());
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::interface_down(Interface* iface)
{
    (void)iface;
    log_debug("stopping interface %s", iface->name().c_str());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::dump_interface(Interface* iface, oasys::StringBuffer* buf)
{
    (void)iface;
    (void)buf;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::init_link(const LinkRef& link, int argc, const char* argv[])
{
    (void)link;
    (void)argc;
    (void)argv;
    log_debug("init link %s", link->nexthop());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    log_debug("ConvergenceLayer::delete_link: link %s deleted", link->name());
}

//----------------------------------------------------------------------
void
ConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    (void)link;
    (void)buf;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::reconfigure_link(const LinkRef& link,
                                   int argc, const char* argv[])
{
    (void)link;
    (void)argv;
    return (argc == 0);
}

//----------------------------------------------------------------------
void
ConvergenceLayer::reconfigure_link(const LinkRef& link,
                                   AttributeVector& params)
{
    (void)link;
    (void)params;
    log_debug("reconfigure link %s", link->name());
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::close_contact(const ContactRef& contact)
{
    (void)contact;
    log_debug("closing contact *%p", contact.object());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::cancel_bundle(const LinkRef& link, const BundleRef& bundle)
{
    (void)link;
    (void)bundle;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::is_eid_reachable(const std::string& query_id,
                                   Interface* iface,
                                   const std::string& endpoint)
{
    (void)iface;
    (void)endpoint;

    EIDReachableReportEvent* event_to_post;
    event_to_post = new EIDReachableReportEvent(query_id, false);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
ConvergenceLayer::query_link_attributes(const std::string& query_id,
                                        const LinkRef& link,
                                        const AttributeNameVector& attributes)
{
    (void)attributes;

    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_debug("ConvergenceLayer::query_link_attributes: "
                  "link %s already deleted", link->name());
        return;
    }

    AttributeVector attrib_values;
    LinkAttributesReportEvent* event_to_post;
    event_to_post = new LinkAttributesReportEvent(query_id, attrib_values);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
ConvergenceLayer::query_iface_attributes(const std::string& query_id,
                                         Interface* iface,
                                         const AttributeNameVector& attributes)
{
    (void)iface;
    (void)attributes;

    AttributeVector attrib_values;
    IfaceAttributesReportEvent* event_to_post;
    event_to_post = new IfaceAttributesReportEvent(query_id, attrib_values);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
ConvergenceLayer::query_cla_parameters(const std::string& query_id,
                                       const AttributeNameVector& parameters)
{
    (void)parameters;

    AttributeVector param_values;
    CLAParametersReportEvent* event_to_post;
    event_to_post = new CLAParametersReportEvent(query_id, param_values);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

//----------------------------------------------------------------------
void
ConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Not implemented for CLA: %s\n", name());
}

/**
 * Get valid interface options for the CLA
 */
//----------------------------------------------------------------------
void
ConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Not implemented for CLA: %s\n", name());
}


} // namespace dtn
