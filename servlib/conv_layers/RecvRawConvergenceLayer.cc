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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#include <oasys/io/IO.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/URI.h>

#include "RecvRawConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

class RecvRawConvergenceLayer::Params RecvRawConvergenceLayer::defaults_;

/******************************************************************************
 *
 * RecvRawConvergenceLayer
 *
 *****************************************************************************/
RecvRawConvergenceLayer::RecvRawConvergenceLayer()
    : ConvergenceLayer("RecvRawConvergenceLayer", "file")
{
    defaults_.can_transmit_ = true;
}


//----------------------------------------------------------------------
CLInfo*
RecvRawConvergenceLayer::new_link_params()
{
    return new RecvRawConvergenceLayer::Params(RecvRawConvergenceLayer::defaults_);
}

/**
 * Bring up a new interface.
 */
bool
RecvRawConvergenceLayer::interface_up(Interface* iface,
                                   int argc, const char* argv[])
{
    (void)iface;
    (void)argc;
    (void)argv;
    return true;
}

//----------------------------------------------------------------------
void
RecvRawConvergenceLayer::interface_activate(Interface* iface)
{
    (void) iface;
}

/**
 * Bring down the interface.
 */
bool
RecvRawConvergenceLayer::interface_down(Interface* iface)
{
    (void)iface;
    return true;
}
 
/**
 * Validate that the contact eid specifies a legit directory.
 */
bool
RecvRawConvergenceLayer::open_contact(const ContactRef& contact)
{
    (void)contact;
    // nothing to do
    return true;
}

/**
 * Close the connnection to the contact.
 */
bool
RecvRawConvergenceLayer::close_contact(const ContactRef& contact)
{
    (void)contact;
    // nothing to do
    return true;
}
    
/**
 * Try to send the bundles queued up for the given contact.
 */
void
RecvRawConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    (void)contact;
    (void)bundle;
    return;
}

void 
RecvRawConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bref)
{
    (void)link;
    (void)bref;
    return;
}

} // namespace dtn
