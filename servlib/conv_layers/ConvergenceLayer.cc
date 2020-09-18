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

#include "ConvergenceLayer.h"
#include "BluetoothConvergenceLayer.h"
#include "EthConvergenceLayer.h"
#include "FileConvergenceLayer.h"
#include "NullConvergenceLayer.h"
#include "SerialConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "NORMConvergenceLayer.h"
#include "LTPConvergenceLayer.h"
#include "AX25CMConvergenceLayer.h"

#ifdef LTPUDP_ENABLED
#    include "LTPUDPConvergenceLayer.h"
#    include "LTPUDPReplayConvergenceLayer.h"
#endif

#include "STCPConvergenceLayer.h"

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
    add_clayer(new SerialConvergenceLayer());
    add_clayer(new TCPConvergenceLayer());
    add_clayer(new UDPConvergenceLayer());
#ifdef __linux__
    add_clayer(new EthConvergenceLayer());
#endif
#ifdef OASYS_BLUETOOTH_ENABLED
    add_clayer(new BluetoothConvergenceLayer());
#endif
#ifdef NORM_ENABLED
    add_clayer(new NORMConvergenceLayer());
#endif
#ifdef LTP_ENABLED
    add_clayer(new LTPConvergenceLayer());
#endif

#ifdef LTPUDP_ENABLED
    add_clayer(new LTPUDPConvergenceLayer());
    add_clayer(new LTPUDPReplayConvergenceLayer());
#endif


#ifdef OASYS_AX25_ENABLED
	add_clayer(new AX25CMConvergenceLayer());
#endif

    add_clayer(new STCPConvergenceLayer());

    // XXX/demmer fixme
    //add_clayer("file", new FileConvergenceLayer());
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
    }
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_cla_parameters(AttributeVector &params)
{
    (void)params;
    log_debug("set cla parameters");
    BundleDaemon::post(new CLAParamsSetEvent(this, ""));
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

    BundleDaemon::post(new EIDReachableReportEvent(query_id, false));
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
    BundleDaemon::post(new LinkAttributesReportEvent(query_id, attrib_values));
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
    BundleDaemon::post(new IfaceAttributesReportEvent(query_id, attrib_values));
}

//----------------------------------------------------------------------
void
ConvergenceLayer::query_cla_parameters(const std::string& query_id,
                                       const AttributeNameVector& parameters)
{
    (void)parameters;

    AttributeVector param_values;
    BundleDaemon::post(new CLAParametersReportEvent(query_id, param_values));
}

} // namespace dtn
