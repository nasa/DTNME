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

#ifndef _OASYS_STACKTRACE_H_
#define _OASYS_STACKTRACE_H_

#include <sys/types.h>

namespace oasys {

class StackTrace {
public:
    /// Print the current stack trace
    static void print_trace_2(bool in_sighandler);
    static void print_current_trace(bool in_sighandler);

    /// Print a stack trace to stderr
    static void print_trace(void *stack[], size_t count);

    /// Get a stack trace (architecture-specific)
    static size_t get_trace(void *stack[], size_t size,
                            u_int sighandler_frame);

    /// Maximum levels for a stack trace
    static const int MAX_STACK_DEPTH = 100;
};

} // namespace oasys


#endif /* _OASYS_STACKTRACE_H_ */
