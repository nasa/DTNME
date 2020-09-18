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

#ifndef _NULLCONVERGENCELAYER_H_
#define _NULLCONVERGENCELAYER_H_

#include "ConvergenceLayer.h"

namespace dtn {

/**
 * The null convergence layer consumes all bundles immediately and
 * does no actual transmission, roughly similar to /dev/null.
 */
class NullConvergenceLayer : public ConvergenceLayer {
public:
    NullConvergenceLayer();

    /// Link parameters
    class Params : public CLInfo {
    public:
        virtual ~Params() {}
        virtual void serialize(oasys::SerializeAction* a);

        /// Whether or not the link can actually send bundles (default true)
        bool can_transmit_;
    };

    /// Default parameters
    static Params defaults_;

    /// @{ Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    bool reconfigure_link(const LinkRef& link, int argc, const char* argv[]);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    void delete_link(const LinkRef& link);
    bool open_contact(const ContactRef& contact);
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);
    void cancel_bundle(const LinkRef& link, const BundleRef& bundle);
    /// @}

private:
    /// Helper function to parse link parameters
    bool parse_link_params(Params* params,
                           int argc, const char** argv,
                           const char** invalidp);
    virtual CLInfo* new_link_params();
};

} // namespace dtn

#endif /* _NULLCONVERGENCELAYER_H_ */
