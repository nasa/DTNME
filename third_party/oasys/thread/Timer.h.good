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


/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef OASYS_SHARED_TIMER_H
#define OASYS_SHARED_TIMER_H

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <queue>
#include <signal.h>
#include <math.h>

#include "../debug/DebugUtils.h"
#include "../debug/Log.h"
#include "../util/Ref.h"
#include "../util/Singleton.h"
#include "../util/Time.h"
#include "MsgQueue.h"
#include "OnOffNotifier.h"
#include "Thread.h"

/**
 * Typedef for a signal handler function. On some (but not all)
 * systems, there is a system-provided __sighandler_t typedef that is
 * equivalent, but in other cases __sighandler_t is a function pointer
 * (not a function).
 */
typedef RETSIGTYPE (sighandlerfn_t) (int);

namespace oasys {

/**
 * Miscellaneous timeval macros.
 */
#define TIMEVAL_DIFF(t1, t2, t3) \
    do { \
       ((t3).tv_sec  = (t1).tv_sec - (t2).tv_sec); \
       ((t3).tv_usec = (t1).tv_usec - (t2).tv_usec); \
       if ((t3).tv_usec < 0) { (t3).tv_sec--; (t3).tv_usec += 1000000; } \
    } while (0)

#define TIMEVAL_DIFF_DOUBLE(t1, t2) \
    ((double)(((t1).tv_sec  - (t2).tv_sec)) + \
     (double)((((t1).tv_usec - (t2).tv_usec)) * 1000000.0))

#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))

#define TIMEVAL_DIFF_USEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000000) + \
     (((t1).tv_usec - (t2).tv_usec)))

#define TIMEVAL_GT(t1, t2) \
    (((t1).tv_sec  >  (t2).tv_sec) ||  \
     (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec > (t2).tv_usec)))

#define TIMEVAL_LT(t1, t2) \
    (((t1).tv_sec  <  (t2).tv_sec) ||  \
     (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec < (t2).tv_usec)))

#define DOUBLE_TO_TIMEVAL(d, tv)                                             \
    do {                                                                     \
        (tv).tv_sec  = static_cast<unsigned long>(floor(d));                 \
        (tv).tv_usec = static_cast<unsigned long>((d - floor(d)) * 1000000); \
    } while (0)

class SpinLock;
class SharedTimer;

typedef std::shared_ptr<SharedTimer> SPtr_Timer;


/**
 * The Timer comparison class.
 */
class SharedTimerCompare {
public:
    inline bool operator ()(SPtr_Timer& a, SPtr_Timer& b);
};    

/**
 * The main Timer system implementation that needs to be driven by a
 * thread, such as the TimerThread class defined below. A thread to
 * clean out cancelled timers can also be invoked, such as the
 * TimerReaperThread class defined below.
 */
class SharedTimerSystem : public Singleton<SharedTimerSystem>,
                    public Logger {
public:
    void schedule_at(struct timeval *when, SPtr_Timer& timer);
    void schedule_in(int milliseconds, SPtr_Timer& timer);
    void schedule_immediate(SPtr_Timer& timer);
    bool cancel(SPtr_Timer& timer);

    /**
     * Hook to use the timer thread to safely handle a signal.
     */
    void add_sighandler(int sig, sighandlerfn_t* handler);

    /**
     * Hook called from an the actual signal handler that notifies the
     * timer system thread to call the signal handler function.
     */
    static void post_signal(int sig);

    /**
     * Run any timers that have expired. Returns the interval in
     * milliseconds until the next timer that needs to fire.
     */
    int run_expired_timers();

    /**
     * Check to see if cancelled timers are accumulating
     * and if so swap timer queues and get rid of them.
     */
    void check_cancelled_timers();

    /**
     * Accessor for the notifier that indicates if another thread put
     * a timer on the queue.
     */
    OnOffNotifier* notifier() { return &notifier_; }

    /**
     * Return a count of the number of pending timers.
     */
    size_t num_pending_timers();

    /**
     * Set flag to stop processing
     */
    virtual void shutdown();

    /**
     * Method to cancel and remove all timers while shutting down
     */
    virtual void cancel_all_timers();

    /**
     * Pause/resume processing of timers 
     * (expected use is pause while shutting down (not sure about need for resume)
     */
    virtual void pause_processing();
    virtual void resume_processing();

private:
    friend class Singleton<SharedTimerSystem>;
    typedef std::priority_queue<SPtr_Timer, 
                                std::vector<SPtr_Timer>, 
                                SharedTimerCompare> SharedTimerQueue;
    

    //! KNOWN ISSUE: Signal handling has race conditions - but it's
    //! not worth the trouble to fix.
    sighandlerfn_t* handlers_[NSIG];  ///< handlers for signals
    bool 	        signals_[NSIG];	  ///< which signals have fired
    bool	        sigfired_;		  ///< boolean to check if any fired

    bool should_stop_;              ///< boolean flag to signal shutdown
    bool pause_processing_;
    SpinLock*  system_lock_;
    SpinLock*  cancel_lock_;
    OnOffNotifier notifier_;
    SharedTimerQueue* timers_;      ///< pointer to current active queue
    SharedTimerQueue* old_timers_;  ///< pointer to queue contaning old cancelled timers
    SharedTimerQueue timer_q1_;     ///< one of two timer queues that can be active
    SharedTimerQueue timer_q2_;     ///< one of two timer queues that can be active
    u_int64_t   seqno_;       ///< seqno used to break ties
    size_t      num_cancelled_; ///< needed for accurate pending_timer count
    size_t      old_num_cancelled_; ///< needed for accurate pending_timer count

    SharedTimerSystem();
    virtual ~SharedTimerSystem();
    
    //dz debug void pop_timer(const struct timeval& now);
    void process_popped_timer(const struct timeval& now, SPtr_Timer& next_timer);

    void handle_signals();

    /**
     * Reinserts a timer into the main timer queue. 
     */
    void reinsert_timer(SPtr_Timer& timer);

    /**
     * Logs and possibly deletes cancelled timers 
     * while processing the old_timers_ queue.
     */
    void process_old_cancelled_timer(SPtr_Timer& timer, 
                                     const struct timeval& now);
};

/**
 * A simple thread class that drives the TimerSystem implementation.
 */
class SharedTimerThread : public Thread {
public:
    static void init();

    static SharedTimerThread* instance() { return instance_; }

    virtual ~SharedTimerThread() {}

    /**
     * Do shutdown processing
     */
    virtual void shutdown();

    /**
     * Pause/resume processing of timers 
     * (expected use is pause while shutting down (not sure about need for resume)
     */
    virtual void pause_processing();
    virtual void resume_processing();

private:
    SharedTimerThread() : Thread("TimerThread", Thread::DELETE_ON_EXIT),
//    SharedTimerThread() : Thread("TimerThread"),
                    pause_processing_(false)
    {}

    void run();
    
    bool pause_processing_;

    static SharedTimerThread* instance_;
};

/**
 * A simple thread class that cleans up cancelled Timers.
 * #define allows conditional compilation in dtn2.
 */
#define HAVE_TIMER_REAPER_THREAD
class SharedTimerReaperThread : public Thread {
public:
    static void init();

    static SharedTimerReaperThread* instance() { return instance_; }

    virtual ~SharedTimerReaperThread() {}

    /**
     * Do shutdown processing
     */
    virtual void shutdown() { set_should_stop(); }

private:
    SharedTimerReaperThread() : Thread("TimerReaperThread", Thread::DELETE_ON_EXIT) {}

    void run();
    
    static SharedTimerReaperThread* instance_;
};

/**
 * A Timer class. Provides methods for scheduling timers. Derived
 * classes must override the pure virtual timeout() method.
 */
class SharedTimer {
public:
    /// Enum type for cancel flags related to memory management
    typedef enum {
        NO_DELETE = 0,
        DELETE_ON_CANCEL = 1
    } cancel_flags_t;

    SharedTimer(cancel_flags_t cancel_flags = DELETE_ON_CANCEL)    // DELETE_ON_CANCEL ignored with shared ptrs
        : cancel_flags_(cancel_flags),
          pending_(false),
          cancelled_(false)
    {}
    
    virtual ~SharedTimer() 
    {
        /*
         * The only time a timer should be deleted is after it fires,
         * so assert as such.
         */
        ASSERTF(pending_ == false, "can't delete a pending timer");
    }
    
    virtual cancel_flags_t cancel_flags()   { return cancel_flags_; }

    virtual void schedule_at(struct timeval *when, SPtr_Timer& sptr);
    virtual void schedule_at(const Time& when, SPtr_Timer& sptr);
    virtual void schedule_in(int milliseconds, SPtr_Timer& sptr);
    virtual void schedule_immediate(SPtr_Timer& sptr);
    virtual bool cancel(SPtr_Timer& sptr);

    virtual void set_cancelled()    { cancelled_ = true; }
    virtual void clear_cancelled()  { cancelled_ = false; }
    virtual bool cancelled() { return cancelled_; }

    virtual void set_pending()   { pending_ = true; }
    virtual void clear_pending() { pending_ = false; }
    virtual bool pending() { return pending_; }

    virtual void set_when()                  { ::gettimeofday(&when_, 0); }
    virtual void set_when(struct timeval* t) { when_ = *t; }
    virtual struct timeval when() { return when_; }

    virtual void       set_seqno(u_int64_t t)  { seqno_ = t; }
    virtual u_int64_t  seqno()                 { return seqno_; }
    
    virtual void timeout(const struct timeval& now) = 0;

protected:
    struct timeval when_;	  ///< When the timer should fire
    cancel_flags_t cancel_flags_; ///< Should we keep the timer around
                                  ///< or delete it when the cancelled
                                  ///< timer bubbles to the top
    u_int64_t      seqno_;        ///< seqno used to break ties

private:
    bool           pending_;	  ///< Is the timer currently pending
    bool           cancelled_;	  ///< Is this timer cancelled
};

/**
 * The Timer comparator function used in the priority queue.
 */
bool
SharedTimerCompare::operator()(SPtr_Timer& a, SPtr_Timer& b)
{
    if (TIMEVAL_GT(a->when(), b->when())) return true;
    if (TIMEVAL_LT(a->when(), b->when())) return false;
    return a->seqno() > b->seqno();
}


/**
 * For use with the QueuingTimer, this struct defines a TimerEvent,
 * i.e. a particular firing of a Timer that captures the timer and the
 * time when it fired.
 */
struct SharedTimerEvent {
    SharedTimerEvent(const SPtr_Timer& timer, struct timeval* time)
        : timer_(timer), time_(*time)
    {
    }
    
    const SPtr_Timer timer_;
    struct timeval time_;
};

/**
 * The queue type used in the QueueingTimer.
 */
typedef MsgQueue<SharedTimerEvent> SharedTimerEventQueue;
    
} // namespace oasys

#endif /* OASYS_SHARED_TIMER_H */

