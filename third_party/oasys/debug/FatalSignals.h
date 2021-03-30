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

#ifndef _FATALSIGNALS_H_
#define _FATALSIGNALS_H_

namespace oasys {

class FatalSignals {
public:
    /**
     * Set up the debugging stack dump handler for all fatal signals
     */
    static void init(const char* appname);

    /**
     * Cancel registered fatal handlers.
     */
    static void cancel();

    /**
     * Set the directory to chdir to before dumping core in the case
     * of a fatal signal.
     */
    static void set_core_dir(const char* dir)
    {
        core_dir_ = dir;
    }

    /**
     * Print a stack trace and die.
     */
    static void die() __attribute__((noreturn));

protected:
    /// Fatal signal handler.
    static void handler(int sig);

    /// The app name to put in the stack trace printout
    static const char* appname_;
    
    /// The directory to chdir() into before dumping core.
    static const char* core_dir_;
    
    /// Flag set in the abort/quit handler when we deliver the signal
    /// to other threads.
    static bool in_abort_handler_;
};

} // namespace oasys


#endif /* _FATALSIGNALS_H_ */
