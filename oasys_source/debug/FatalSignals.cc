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

#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "FatalSignals.h"
#include "StackTrace.h"
#include "thread/Thread.h"

namespace oasys {

const char* FatalSignals::appname_  = "(unknown app)";
const char* FatalSignals::core_dir_ = NULL;
bool        FatalSignals::in_abort_handler_ = false;

void
FatalSignals::init(const char* appname)
{
    appname_ = appname;

#ifndef __CYGWIN__
    signal(SIGSEGV, FatalSignals::handler);
    signal(SIGBUS,  FatalSignals::handler);
    signal(SIGILL,  FatalSignals::handler);
    signal(SIGFPE,  FatalSignals::handler);
    signal(SIGABRT, FatalSignals::handler);
    signal(SIGQUIT, FatalSignals::handler);
#endif
}

void
FatalSignals::cancel()
{
#ifndef __CYGWIN
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS,  SIG_DFL);
    signal(SIGILL,  SIG_DFL);
    signal(SIGFPE,  SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
#endif
}

void
FatalSignals::handler(int sig)
{
    const char* signame = "";
    switch(sig) {
#define FATAL(_s) case _s: signame = #_s; break;

    FATAL(SIGSEGV);
    FATAL(SIGBUS);
    FATAL(SIGILL);
    FATAL(SIGFPE);
    FATAL(SIGABRT);
    FATAL(SIGQUIT);

    default:
        char buf[1024];
        snprintf(buf, sizeof(buf), "ERROR: UNEXPECTED FATAL SIGNAL %d\n", sig);
        exit(1);
    };
    
    fprintf(stderr, "ERROR: %s (pid %d) got fatal %s - will dump core\n",
            appname_, (int)getpid(), signame);

    // make sure we're in the right directory
    if (!in_abort_handler_ && core_dir_ != NULL) {
        fprintf(stderr, "fatal handler chdir'ing to core dir '%s'\n",
                core_dir_);
        chdir(core_dir_);   
    }

    StackTrace::print_current_trace(true);
    fflush(stderr);

    // trap-generated signals are automatically redelivered by the OS,
    // so we restore the default handler below.
    //
    // SIGABRT and SIGQUIT, however, need to be redelivered. but, we
    // really would like to have stack traces from all threads, so we
    // first set a flag indicating that we've started the process,
    // then try to deliver the same signal to all the other threads to
    // try to get more stack traces
    if (sig == SIGABRT || sig == SIGQUIT) {
        if (! in_abort_handler_) {
            in_abort_handler_ = true;

#ifndef __CYGWIN__
            Thread** ids = Thread::all_threads_;
            for (int i = 0; i < Thread::max_live_threads_; ++i) {

                if ((ids[i] != NULL) &&
                    ids[i]->thread_id() != Thread::current())
                {
                    ThreadId_t thread_id = ids[i]->thread_id();
                    fprintf(stderr,
                            "fatal handler sending signal to thread %p\n",
                            (void*)thread_id);
                    pthread_kill(thread_id, sig);
                    sleep(1);
                }
            }
#endif
            
            fprintf(stderr, "fatal handler dumping core\n");
            signal(sig, SIG_DFL);
            kill(getpid(), sig);
        }
    } else {
        signal(sig, SIG_DFL);

        // Cygwin doesn't redeliver the signal automatically
#ifdef __CYGWIN__
        kill(getpid(), sig);
#endif
    }
}

void
FatalSignals::die()
{
    Breaker::break_here();
    StackTrace::print_current_trace(false);
    if (core_dir_ != NULL) {
        fprintf(stderr, "fatal handler chdir'ing to core dir '%s'\n",
                core_dir_);
        chdir(core_dir_);
    }

    cancel();
    ::abort();
}


} // namespace oasys
