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

#ifndef _OASYS_THREAD_H_
#define _OASYS_THREAD_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#include <sys/types.h>

#ifdef __win32__

#include <Windows.h>

#else

#include <pthread.h>
#include <signal.h>

#ifdef HAVE_SCHED_YIELD
#include <sched.h>
#endif

#endif // __win32__

#include <vector>

#include "../debug/DebugUtils.h"
#include "../thread/LockDebugger.h"
#include "../thread/TLS.h"

namespace oasys {

#ifdef __win32__
typedef DWORD     ThreadId_t;
#else
typedef pthread_t ThreadId_t;
#endif

class SpinLock;

/**
 * Class to wrap a thread of execution using native system threads
 * (i.e. pthreads on UNIX or Windows threads). Similar to the Java
 * API.
 */
class Thread {
public:
    /**
     * Bit values for thread flags.
     */
    enum thread_flags_t { 
        //! inverse of PTHREAD_CREATE_DETACHED. Implies
        //! not DELETE_ON_EXIT.
        CREATE_JOINABLE	= 1 << 0,
        //! delete thread when run() completes. 
        DELETE_ON_EXIT  = 1 << 1,
        //! thread can be interrupted
        INTERRUPTABLE   = 1 << 2,
        //! thread has been started
        STARTED         = 1 << 3,
        //! bit to signal the thread to stop
        SHOULD_STOP   	= 1 << 4,
        //! bit indicating the thread has stopped
        STOPPED   	= 1 << 5,
    };

#ifndef __win32__
    /**
     * Enum to define signals used internally in the thread system
     * (currently just the interrupt signal).
     */
    enum thread_signal_t {
        INTERRUPT_SIG = SIGURG
    };
#endif

    /**
     * Maximum number of live threads.
     */
    static const int max_live_threads_ = 256;

    /**
     * Array of all live threads. Used for debugging, see
     * FatalSignals.cc.
     */
    static Thread* all_threads_[Thread::max_live_threads_];

    /**
     * Spin lock to protect the all threads array on thread startup /
     * shutdown.
     */
    static SpinLock* all_threads_lock_;

    /**
     * Activate the thread creation barrier. No new threads will be
     * created until release_start_barrier() is called.
     *
     * Note this should only be called in a single-threaded context,
     * i.e. during initialization.
     */
    static void activate_start_barrier();
    
    /**
     * Release the thread creation barrier and actually start up any
     * pending threads.
     */
    static void release_start_barrier();
    
    /**
     * @return Status of start barrier
     */
    static bool start_barrier_enabled() { 
        return start_barrier_enabled_;
    }

    /**
     * Yield the current process. Needed for capriccio to yield the
     * processor during long portions of code with no I/O. (Probably
     * used for testing only).
     */
    static void yield();

    /**
     * Potentially yield the current thread to implement a busy
     * waiting function.
     *
     * The implementation is dependant on the configuration. On an
     * SMP, don't do anything. Otherwise, with capriccio threads, this
     * should never happen, so issue an assertion. Under pthreads,
     * however, actually call thread_yield() to give up the processor.
     */
    static void spin_yield();

    /**
     * Return a pointer to the currently running thread.
     *
     * XXX/demmer this could keep a map to return a Thread* if it's
     * actually useful
     */
    static ThreadId_t current();

    /**
     * @return True if ThreadId_t's are equal
     */
    static bool id_equal(ThreadId_t a, ThreadId_t b);

    /**
     * Constructor
     */ 
    Thread(const char* name, int flags = 0);

    /**
     * Destructor
     */
    virtual ~Thread();
    
    /**
     * Starts a new thread and calls the virtual run() method with the
     * new thread.
     */
    void start();

    /**
     * Join with this thread, blocking the caller until we quit.
     */
    void join();

    /**
     * Send a signal to the thread.
     */
    void kill(int sig);

    /**
     * Send an interrupt signal to the thread to unblock it if it's
     * stuck in a system call. Implemented with SIGUSR1.
     *
     * XXX/bowei - There's no good way to implement this on many
     * platforms. We should get rid of this.
     */
    void interrupt();

    /**
     * Set the interruptable state of the thread (default off). If
     * interruptable, a thread can receive SIGUSR1 signals to
     * interrupt any system calls.
     *
     * Note: This must be called by the thread itself. To set the
     * interruptable state before a thread is created, use the
     * INTERRUPTABLE flag.
     */
    void set_interruptable(bool interruptable);
    
    /**
     * Set a bit to stop the thread
     *
     * This simply sets a bit in the thread's flags that can be
     * examined by the run method to indicate that the thread should
     * be stopped.
     */
    void set_should_stop() { flags_ |= SHOULD_STOP; }

    /**
     * Return whether or not the thread should stop.
     */
    bool should_stop() { return ((flags_ & SHOULD_STOP) != 0); }
    
    /**
     * Return true if the thread has stopped.
     */
    bool is_stopped() { return ((flags_ & STOPPED) != 0); }

    /**
     * Return true if the thread has been started.
     */
    bool started() { return ((flags_ & STARTED) != 0); }

    /**
     * Set the given thread flag.
     */
    void set_flag(thread_flags_t flag) { flags_ |= flag; }

    /**
     * Clear the given thread flag.
     */
    void clear_flag(thread_flags_t flag) { flags_ &= ~flag; }
    
    /**
     * Return a pointer to the Thread object's id. The static function
     * current() is the currently executing thread id.
     */
    ThreadId_t thread_id() { return thread_id_; }

#if OASYS_DEBUG_LOCKING_ENABLED
    static LockDebugger* lock_debugger() {
        return lock_debugger_.get(); 
    }
#endif

    /*
     * The name of the thread.
     */
    const char * threadName() { return((char *) name_); }

protected:
#ifdef __win32__
    //! Declare a current Thread* in thread local storage
    static __declspec(thread) Thread* current_thread_;
    HANDLE join_event_;
#endif // __win32__

    static bool     globals_inited_;
    
#ifndef __win32__
    static sigset_t interrupt_sigset_;
#endif    

    // boolean to avoid performing DELETE_ON_EXIT
    // before thread setup has completed
    bool setup_in_progress_;

    static bool                 start_barrier_enabled_;
    static std::vector<Thread*> threads_in_barrier_;

#if OASYS_DEBUG_LOCKING_ENABLED
    // Locking debugging thread - local to each thread
    static TLS<LockDebugger> lock_debugger_;
#endif

    ThreadId_t thread_id_;
    volatile int	flags_;
    char       name_[64];

    /**
     * Static function which is given to the native threading library.
     */
#ifdef __win32__
    static DWORD WINAPI pre_thread_run(void* t);
#else
    static void* pre_thread_run(void* t);
#endif

    /**
     * Noop thread signal handler.
     */
    static void interrupt_signal(int sig);

    /**
     * Wrapper function to handle initialization and cleanup of the
     * thread. Runs in the context of the newly created thread.
     */
    void thread_run(const char* thread_name, ThreadId_t thread_id);

    /**
     * Derived classes should implement this function which will get
     * called in the new Thread context.
     */
    virtual void run() = 0;

    /**
     * Helper class needed to boostrap initialize the thread system.
     */
    friend struct GlobalThreadInit;
};

//---------------------------------------------------------------------------
inline ThreadId_t
Thread::current()
{
#ifdef __win32__
    return current_thread_->thread_id_;
#else
    return pthread_self();
#endif
}

//---------------------------------------------------------------------------
inline void
Thread::yield()
{
#ifndef __win32__
#ifdef HAVE_THREAD_ID_YIELD
    thread_id_yield();
#elif  HAVE_SCHED_YIELD
    sched_yield();
#else
#error MUST EITHER HAVE THREAD_ID_YIELD OR SCHED_YIELD
#endif
#endif // __win32__
}

//---------------------------------------------------------------------------
inline void
Thread::spin_yield()
{
    // XXX/bowei: 
    // 1-p call yield()
    // o.w. spin
    Thread::yield();
}

} // namespace oasys

#endif /* _OASYS_THREAD_H_ */
