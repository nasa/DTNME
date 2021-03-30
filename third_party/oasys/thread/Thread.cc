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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <errno.h>

#include "Thread.h"
#include "SpinLock.h"

#include "../debug/DebugUtils.h"
#include "../debug/Log.h"
#include "../util/CString.h"

#if GOOGLE_PROFILE_ENABLED
#include <google/profiler.h>
#endif

namespace oasys {

struct GlobalThreadInit {
    GlobalThreadInit();
};

GlobalThreadInit g;

#ifdef __win32__
__declspec(thread) Thread* Thread::current_thread_ = 0;
#else
sigset_t             Thread::interrupt_sigset_;
#endif

bool                 Thread::start_barrier_enabled_ = false;
std::vector<Thread*> Thread::threads_in_barrier_;

const int            Thread::max_live_threads_;
Thread*              Thread::all_threads_[max_live_threads_];
SpinLock             g_all_threads_lock_;
SpinLock*            Thread::all_threads_lock_ = &g_all_threads_lock_;

#if OASYS_DEBUG_LOCKING_ENABLED
TLS<LockDebugger>        Thread::lock_debugger_;
template<> pthread_key_t TLS<LockDebugger>::key_ = 0;
#endif

//----------------------------------------------------------------------------
void
Thread::activate_start_barrier()
{
    start_barrier_enabled_ = true;
    log_debug_p("/thread", "activating thread creation barrier");
}

//----------------------------------------------------------------------------
void
Thread::release_start_barrier()
{
    start_barrier_enabled_ = false;

    log_debug_p("/thread",
                "releasing thread creation barrier -- %zu queued threads",
                threads_in_barrier_.size());
    
    for (size_t i = 0; i < threads_in_barrier_.size(); ++i) 
    {
        Thread* thr = threads_in_barrier_[i];
        thr->start();
    }

    threads_in_barrier_.clear();
}

//----------------------------------------------------------------------------
bool
Thread::id_equal(ThreadId_t a, ThreadId_t b)
{
#ifdef __win32__
    // XXX/bowei - check if comparing handles for equality is ok
    return a == b;
#else
    return pthread_equal(a, b);
#endif
}

//----------------------------------------------------------------------------
Thread::Thread(const char* name, int flags)
    : flags_(flags)
{
        setup_in_progress_ = true;
    if ((flags & CREATE_JOINABLE) && (flags & DELETE_ON_EXIT)) {
        flags &= ~DELETE_ON_EXIT;
    }

#ifdef __win32__
    if (flags & CREATE_JOINABLE) {
        join_event_ = CreateEvent(0, TRUE, FALSE, 0);
        ASSERT(join_event_ != 0);
    } else {
        join_event_ = 0;
    }
#endif //__win32__
    cstring_copy(name_, 64, name);
    thread_id_ = 0;
}

//----------------------------------------------------------------------------
Thread::~Thread()
{
#ifdef __win32__
    if (join_event_ != 0) {
        CloseHandle(join_event_);
    }
#endif //__win32__    
}

//----------------------------------------------------------------------
GlobalThreadInit::GlobalThreadInit()
{
#ifndef __win32__
    sigemptyset(&Thread::interrupt_sigset_);
    sigaddset(&Thread::interrupt_sigset_, Thread::INTERRUPT_SIG);
    signal(Thread::INTERRUPT_SIG, Thread::interrupt_signal);
#  if !defined(__QNXNTO__) // QNX v6.x declares but does not define siginterrupt.
    siginterrupt(Thread::INTERRUPT_SIG, 1);
#  endif
#endif

#ifdef OASYS_DEBUG_LOCKING_ENABLED
    TLS<LockDebugger>::init();
    TLS<LockDebugger>::set(new LockDebugger());
#endif
}

//----------------------------------------------------------------------------
void
Thread::start()
{
    // check if the creation barrier is enabled
    if (start_barrier_enabled_) 
    {
        log_debug_p("/thread", "delaying start of thread %p [%s] due to barrier",
                    this, name_);
        threads_in_barrier_.push_back(this);
        return;
    }

    log_debug_p("/thread", "starting thread %p [%s]",
                this, name_);

#ifdef __win32__

    DWORD  thread_dword_id;
    HANDLE h_thread = CreateThread(0, // Security attributes
                                   0, // default stack size
                                   Thread::pre_thread_run, // thread function
                                   this,
                                   // start suspended to fix some state in
                                   // the Thread* class before it starts running
                                   CREATE_SUSPENDED,       
                                   &thread_dword_id);
    
    ASSERT(h_thread != 0);
    thread_id_ = thread_dword_id;

    // Fix the handle on the thread
    ResumeThread(h_thread);
#else 

    // If the thread create fails, we retry once every 100ms for up to
    // a minute before panic'ing. This allows a temporary resource
    // shortage (typically memory) to clean itself up before dying
    // completely.
    int ntries = 0;
    while (pthread_create(&thread_id_, 0, Thread::pre_thread_run, this) != 0) 
    {
        if (++ntries == 600) {
            PANIC("maximum thread creation attempts");
#ifdef OASYS_DEBUG_MEMORY_ENABLED
            DbgMemInfo::debug_dump();
#endif
        }
        
        logf("/thread", LOG_ERR, "error in thread_id_create: %s, retrying in 100ms",
             strerror(errno));
        usleep(100000);
    }

    // since in most cases we want detached threads, users must
    // explicitly request for them to be joinable
    if (! (flags_ & CREATE_JOINABLE)) 
    {
        pthread_detach(thread_id_);
    }

#endif // __win32__
    setup_in_progress_ = false;

    log_debug_p("/thread", "started thread: [ %08X -- %s]",
                (u_int32_t) thread_id_, name_);
}

//----------------------------------------------------------------------------
void
Thread::join()
{
    if (! (flags_ & CREATE_JOINABLE)) 
    {
        PANIC("tried to join a thread that isn't joinable -- "
              "need CREATE_JOINABLE flag");
    }
    
#ifdef __win32__
    ASSERT(join_event_ != 0);
    DWORD ret = WaitForSingleObject(join_event_, INFINITE);
    (void)ret; // XXX/bowei - get rid of me
    ASSERT(ret != WAIT_FAILED);
#else
    void* ignored;
    int err;
    if ((err = pthread_join(thread_id_, &ignored)) != 0) 
    {
        PANIC("error in pthread_join: %s", strerror(err));
    }
#endif
}

//----------------------------------------------------------------------------
void
Thread::kill(int sig)
{
#ifdef __win32__
    (void)sig;
    NOTIMPLEMENTED;
#else
    if (pthread_kill(thread_id_, sig) != 0) {
        PANIC("error in pthread_kill: %s", strerror(errno));
    }
#endif
}

//----------------------------------------------------------------------------
void
Thread::interrupt()
{
#ifdef __win32__
    NOTIMPLEMENTED;
#else
    log_debug_p("/thread", "interrupting thread %p [%s]",
                this, name_);
    kill(INTERRUPT_SIG);
#endif
}

//----------------------------------------------------------------------------
void
Thread::set_interruptable(bool interruptable)
{
#ifdef __win32__
    (void)interruptable;
    NOTIMPLEMENTED;
#else
    ASSERT(Thread::current() == thread_id_);
    
    int block = (interruptable ? SIG_UNBLOCK : SIG_BLOCK);
    if (pthread_sigmask(block, &interrupt_sigset_, NULL) != 0) {
        PANIC("error in thread_id_sigmask");
    }
#endif
}


//----------------------------------------------------------------------------
#ifdef __win32__
DWORD WINAPI 
#else
void*
#endif
Thread::pre_thread_run(void* t)
{
    Thread* thr = static_cast<Thread*>(t);

#ifdef __win32__
    current_thread_ = thr;
#endif

    ThreadId_t thread_id = Thread::current();

#if OASYS_DEBUG_LOCKING_ENABLED
    // Set the thread local debugger
    lock_debugger_.set(new LockDebugger());
#endif

    thr->thread_run(thr->name_, thread_id);
    
    return 0;
}

//----------------------------------------------------------------------------
void
Thread::interrupt_signal(int sig)
{
    (void)sig;
}

//----------------------------------------------------------------------------
void
Thread::thread_run(const char* thread_name, ThreadId_t thread_id)
{
    /*
     * The only reason we pass the name to thread_run is to make it
     * appear in the gdb backtrace to more easily identify the actual
     * thread.
     */
    (void)thread_name;
    
#if GOOGLE_PROFILE_ENABLED
    ProfilerRegisterThread();
#endif

    all_threads_lock_->lock("thread startup");
    for (int i = 0; i < max_live_threads_; ++i) {
        if (all_threads_[i] == NULL) {
            all_threads_[i] = this;
            break;
        }
    }
    all_threads_lock_->unlock();

#ifndef __win32__    
    /*
     * There's a potential race between the starting of the new thread
     * and the storing of the thread id in the thread_id_ member
     * variable, so we can't trust that it's been written by our
     * spawner. So we re-write it here to make sure that it's valid
     * for the new thread (specifically for set_interruptable's
     * assertion.
     */
    thread_id_ = thread_id;
    set_interruptable((flags_ & INTERRUPTABLE));
#endif

    flags_ |= STARTED;
    flags_ &= ~STOPPED;
    flags_ &= ~SHOULD_STOP;

    try 
    {
        run();
    } 
    catch (...) 
    {
        PANIC("unexpected exception caught from Thread::run");
    }
    
    flags_ |= STOPPED;

#ifdef __win32__
    if (join_event_) {
        SetEvent(join_event_);
    }
#endif //__win32__

    all_threads_lock_->lock("thread startup");
    for (int i = 0; i < max_live_threads_; ++i) {
        if (all_threads_[i] == this) {
            all_threads_[i] = NULL;
            break;
        }
    }
    all_threads_lock_->unlock();
    
    if (flags_ & DELETE_ON_EXIT) 
    {
        while(setup_in_progress_)
        {
                usleep(100000);
        }
        delete this;
    }

#ifdef __win32__

    // Make sure C++ cleanup is called, which does not occur if we
    // call ExitThread().
    return;

#else

    pthread_exit(0);
    NOTREACHED;

#endif
}

} // namespace oasys
