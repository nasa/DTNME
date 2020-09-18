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

#ifndef _SIM_CL_INFO_
#define _SIM_CL_INFO_

#include "conv_layers/ConvergenceLayer.h"

namespace dtnsim {

class SimCLInfo : public CLInfo {

public:
    SimCLInfo(int id) : CLInfo(),simid_(id) {}
    int id () { return simid_; }
    
private:
    int simid_; // id used to identify this cl globally
        
};
} // namespace dtnsim

#endif /* _SIM_CL_INFO_ */
