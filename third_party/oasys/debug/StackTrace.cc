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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "StackTrace.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <execinfo.h>

namespace oasys {

void
StackTrace::print_trace_2(bool in_sighanlder)
{
    (void) in_sighanlder;

    char buf[1024];
    void *trace[32];
    char **messages = (char**) NULL;
    int i, trace_size=0;

    trace_size = backtrace(trace, 32);
    messages = backtrace_symbols(trace, trace_size);
    strcpy(buf, "[bt] Execution path:\n");
    write(2, buf, strlen(buf));
    for ( i=0; i<trace_size; i++ ) {
        sprintf(buf, "[bt] %s\n", messages[i]);
        write(2, buf, strlen(buf));
    }
}

void
StackTrace::print_current_trace(bool in_sighandler)
{
    void *stack[MAX_STACK_DEPTH];
    memset(stack, 0, sizeof(stack));
    size_t count = get_trace(stack, MAX_STACK_DEPTH, in_sighandler ? 3 : 0);
    if (count > 0) {
        print_trace(stack + 2, count - 2); // skip this fn
    } else {
        char buf[1024];
        strncpy(buf, "NO STACK TRACE AVAILABLE ON THIS ARCHITECTURE\n",
                sizeof(buf));
        write(2, buf, strlen(buf));
    }
}

void
StackTrace::print_trace(void *stack[], size_t count)
{
    char buf[1024];
    void* addr;
    
    strncpy(buf, "STACK TRACE: ", sizeof(buf));
    write(2, buf, strlen(buf));

    for (size_t i = 0; i < count; ++i) {
        addr = stack[i];

#if HAVE_DLADDR
        Dl_info info;
        if (dladdr(addr, &info)) {
            int dll_offset = (int)((char*)addr - (char*)info.dli_fbase);
            sprintf(buf, "0x%08x:%s@0x%08x+0x%08x ",
                    (u_int)addr, info.dli_fname,
                    (u_int)info.dli_fbase, dll_offset);
        } else
#endif
        {
            sprintf(buf, "%p ", addr);
        }
        
        write(2, buf, strlen(buf));
    }
    write(2, "\n", 1);
}

#if defined(__i386__)
#include "StackTrace-x86.cc"

#elif defined(__POWERPC__) || defined(PPC)
#include "StackTrace-ppc.cc"

#elif HAVE_EXECINFO_H

// Stack trace function using the glibc builtin
#include <execinfo.h>
size_t
StackTrace::get_trace(void* stack[], size_t size, u_int sighandler_frame)
{
    (void)sighandler_frame;
    return backtrace(stack, size);
}

#else 

// Last resort -- just an no-op
size_t
StackTrace::get_trace(void* stack[], size_t size, u_int sighandler_frame)
{
    (void)sighandler_frame;
    memset(stack, 0, size);
    return 0;
}

#endif

} // namespace oasys
