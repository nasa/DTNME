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

#ifndef _DAEMONIZER_H_
#define _DAEMONIZER_H_

namespace oasys {

/**
 * A simple utility class used for forking a daemon copy of a process
 * to complete initialization then run in the background.
 *
 * Since we want to enable the forked child to complete some
 * initialization and then return a status, the fork_child() method
 * blocks until the child process calls notify_parent() and sends the
 * initialization status over a pipe.
 */
class Daemonizer {
public:
    /**
     * Called by the parent process to fork a child process and
     * optionally wait for it to return.
     *
     * If wait_for_notify is false, then the parent immediately exits.
     * If true, then it waits for the child to report a status, then
     * calls exit() with the given status. The child continues on in
     * both cases.
     */
    void daemonize(bool wait_for_notify);
    
    /**
     * Called by the child process to signal the parent with its
     * initialization status.
     */
    void notify_parent(int status);

private:
    int pipe_[2];
};

} // namespace oasys

#endif /* _DAEMONIZER_H_ */
