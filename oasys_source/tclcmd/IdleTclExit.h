/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _OASYS_IDLE_TCL_EXIT_H_
#define _OASYS_IDLE_TCL_EXIT_H_

#include "../debug/Logger.h"
#include "../thread/Notifier.h"
#include "../thread/Timer.h"

namespace oasys {

/**
 * Class to facilitate cleanly exiting the tcl event or command loop
 * when the system is idle, based on a virtual is_idle() callback.
 *
 * To be both efficient and thread-safe, the implementation creates a
 * notifier so that tcl can block an event on one side of the pipe and
 * will then call exit_event_loop from within the tcl event loop
 * thread. The class itself is a timer that periodically checks
 * whether is_idle() is true and then triggers the exit sequence when
 * it is true.
 */
class IdleTclExit : public Logger,
                    public Timer {
public:
    IdleTclExit(u_int interval);
    
    virtual bool is_idle(const struct timeval& now) = 0;

    void reschedule();
    void timeout(const struct timeval& now);
    
protected:
    Notifier notifier_;
    u_int    interval_;
};

} // namespace oasys

#endif /* _OASYS_IDLE_TCL_EXIT_H_ */
