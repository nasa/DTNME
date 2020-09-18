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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <oasys/debug/Log.h>
#include <vector>

namespace dtn {

class ConvergenceLayer;
class CLInfo;

/**
 * Abstraction of a local dtn interface.
 *
 * Generally, interfaces are created by the configuration file /
 * console.
 */
class Interface {
public:
    // Accessors
    const std::string& name()    const { return name_; }
    const std::string& proto()   const { return proto_; }
    ConvergenceLayer*  clayer()  const { return clayer_; }
    CLInfo*	       cl_info() const { return cl_info_; }

    /**
     * Store the ConvergenceLayer specific state.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }

protected:
    friend class InterfaceTable;
    
    Interface(const std::string& name,
              const std::string& proto,
              ConvergenceLayer* clayer);
    ~Interface();
    
    std::string name_;		///< Name of the interface
    std::string proto_;		///< What type of CL
    ConvergenceLayer* clayer_;	///< Convergence layer to use
    CLInfo* cl_info_;		///< Convergence layer specific state
};

} // namespace dtn

#endif /* _INTERFACE_H_ */
