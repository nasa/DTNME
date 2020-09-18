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

#ifndef _RECVRAW_CONVERGENCE_LAYER_H_
#define _RECVRAW_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"
#include <oasys/thread/Thread.h>

namespace dtn {

class RecvRawConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    RecvRawConvergenceLayer();

    /// Link parameters
    class Params : public CLInfo {
    public:
        virtual ~Params() {}

        /// Whether or not the link can actually send bundles (default true)
        bool can_transmit_;
    };

    /// Default parameters
    static Params defaults_;

    /// @{
    virtual bool interface_up(Interface* iface,
                              int argc, const char* argv[]);
    virtual void interface_activate(Interface* iface);
    virtual bool interface_down(Interface* iface);
    virtual bool open_contact(const ContactRef& contact);
    virtual bool close_contact(const ContactRef& contact);
    virtual void send_bundle(const ContactRef& contact, Bundle* bundle);

    void bundle_queued(const LinkRef& link, const BundleRef& bref);

protected:
    /**
     * Current version of the file cl protocol.
     */
    static const int CURRENT_VERSION = 0x1;
    
    /// @{ virtual from ConvergenceLayer
    virtual CLInfo* new_link_params();
    /// @}
    
};

} // namespace dtn

#endif /* _RECVRAW_CONVERGENCE_LAYER_H_ */
