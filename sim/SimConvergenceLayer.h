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

#ifndef _SIM_CONVERGENCE_LAYER_H_
#define _SIM_CONVERGENCE_LAYER_H_


#include "conv_layers/ConvergenceLayer.h"

using namespace dtn;

namespace dtnsim {

struct ConnState;
class Node;

/**
 * Simulator implementation of the Convergence Layer API.
 */
class SimConvergenceLayer : public ConvergenceLayer {
    
public:
    /**
     * Singleton initializer.
     */
    static void init()
    {
        instance_ = new SimConvergenceLayer();
    }

    /**
     * Singleton accessor
     */
    static SimConvergenceLayer* instance() { return instance_; }

    /**
     * Constructor.
     */
    SimConvergenceLayer();

    /// @{
    /// Virtual from ConvergenceLayer
    virtual CLInfo *new_link_params(void) { return NULL; }
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    void delete_link(const LinkRef& link);
    bool open_contact(const ContactRef& contact);
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);
    /// @}

    void update_connectivity(Node* n1, Node* n2, const ConnState& cs);
    
protected:
    void start_bundle(const LinkRef& link, const BundleRef& bundle);
    
    static SimConvergenceLayer* instance_;
};

} // namespace dtnsim

#endif /* _SIM_CONVERGENCE_LAYER_H_ */
