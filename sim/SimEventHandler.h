/*
 *    Copyright 2005-2006 Intel Corporation
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


#ifndef _DTN_SIM_EVENT_HANDLER_H_
#define _DTN_SIM_EVENT_HANDLER_H_

namespace dtnsim {

class SimEvent;

/**
 * Interface implemented by all objects that handle simulator events
 */
class SimEventHandler {
public:
    virtual void process(SimEvent* e) = 0;
    virtual ~SimEventHandler() {}
};

} // namespace dtnsim

#endif /* _DTN_SIM_EVENT_HANDLER_H_ */
