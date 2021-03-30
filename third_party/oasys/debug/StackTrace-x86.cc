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

#if defined(__i386__)

#include "FatalSignals.h"

#if defined(__linux__)
#include <asm/sigcontext.h>
struct sigframe
{
        char *pretcode;
        int sig;
        struct sigcontext sc;
        struct _fpstate fpstate;
};
#endif

// Generally, this function just follows the frame pointer and looks
// at the return address (i.e. the next one on the stack).
//
// Things get a little funky in the frame for the signal handler
// (identified by the parameter sighandler_frame), where we need to
// look into the place where the kernel stored the faulting address.
size_t
StackTrace::get_trace(void* stack[], size_t size, u_int sighandler_frame)
{
    void **fp;

    asm volatile("movl %%ebp,%0" : "=r" (fp));

    stack[0] = 0; // fake frame for this this fn, just use 0
    size_t frame = 1;
    while (frame < size) {
        if (*(fp + 1) == 0 || *fp == 0)
            break;
        
        if (sighandler_frame != 0 && frame == sighandler_frame) {
#if defined(__linux__) 
            struct sigframe* sf = (struct sigframe*)(fp+1);
            struct sigcontext* scxt = &(sf->sc);
            stack[frame] = (void*) scxt->eip;
#else
            stack[frame] = *(fp + 1);
#endif 
        } else {
            stack[frame] = *(fp + 1);
        }
        
        fp = (void **)(*fp);
        ++frame;
    }

    return frame;
}

#endif /* __i386__ */
