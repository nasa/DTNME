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

#ifndef OASYS_TIMER_H
#define OASYS_TIMER_H

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
class Timer;

/**
 * The Timer comparison class.
 */
class TimerCompare {
public:
    inline bool operator ()(Timer* a, Timer* b);
};    

/**
 * The main Timer system implementation that needs to be driven by a
 * thread, such as the TimerThread class defined below. A thread to
 * clean out cancelled timers can also be invoked, such as the
 * TimerReaperThread class defined below.
 */
class TimerSystem : public Singleton<TimerSystem>,
                    public Logger {
public:
    void schedule_at(struct timeval *when, Timer* timer);
    void schedule_in(int milliseconds, Timer* timer);
    void schedule_immediate(Timer* timer);
    bool cancel(Timer* timer);

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
     * Allow SafeTimer to inform that it was cancelled
     */
    virtual void cancelled_safe_timer();

    /**
     * Pause/resume processing of timers 
     * (expected use is pause while shutting down (not sure about need for resume)
     */
    virtual void pause_processing();
    virtual void resume_processing();

private:
    friend class Singleton<TimerSystem>;
    typedef std::priority_queue<Timer*, 
                                std::vector<Timer*>, 
                                TimerCompare> TimerQueue;
    

    //! KNOWN ISSUE: Signal handling has race conditions - but it's
    //! not worth the trouble to fix.
    sighandlerfn_t* handlers_[NSIG];	///< handlers for signals
    bool 	    signals_[NSIG];	///< which signals have fired
    bool	    sigfired_;		///< boolean to check if any fired

    bool            should_stop_;       ///< boolean flag to signal shutdown
    bool pause_processing_;
    SpinLock*  system_lock_;
    SpinLock*  cancel_lock_;
    OnOffNotifier notifier_;
    TimerQueue* timers_;      ///< pointer to current active queue
    TimerQueue* old_timers_;  ///< pointer to queue contaning old cancelled timers
    TimerQueue timer_q1_;     ///< one of two timer queues that can be active
    TimerQueue timer_q2_;     ///< one of two timer queues that can be active
    u_int32_t   seqno_;       ///< seqno used to break ties
    size_t      num_cancelled_; ///< needed for accurate pending_timer count
    size_t      old_num_cancelled_; ///< needed for accurate pending_timer count

    TimerSystem();
    virtual ~TimerSystem();
    
    //dz debug void pop_timer(const struct timeval& now);
    void process_popped_timer(const struct timeval& now, Timer* next_timer);

    void handle_signals();

    /**
     * Reinserts a timer into the main timer queue. 
     */
    void reinsert_timer(Timer* timer);

    /**
     * Logs and possibly deletes cancelled timers 
     * while processing the old_timers_ queue.
     */
    void process_old_cancelled_timer(Timer* timer, 
                                     const struct timeval& now);
};

/**
 * A simple thread class that drives the TimerSystem implementation.
 */
class TimerThread : public Thread {
public:
    static void init();

    static TimerThread* instance() { return instance_; }

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
    TimerThread() : Thread("TimerThread", Thread::DELETE_ON_EXIT),
                    pause_processing_(false)                       {}

    void run();
    
    bool pause_processing_;

    static TimerThread* instance_;
};

/**
 * A simple thread class that cleans up cancelled Timers.
 * #define allows conditional compilation in dtnme.
 */
#define HAVE_TIMER_REAPER_THREAD
class TimerReaperThread : public Thread {
public:
    static void init();

    static TimerReaperThread* instance() { return instance_; }

    /**
     * Do shutdown processing
     */
    virtual void shutdown() { set_should_stop(); }

private:
    TimerReaperThread() : Thread("TimerReaperThread", Thread::DELETE_ON_EXIT) {}

    void run();
    
    static TimerReaperThread* instance_;
};

/**
 * A Timer class. Provides methods for scheduling timers. Derived
 * classes must override the pure virtual timeout() method.
 */
class Timer {
public:
    /// Enum type for cancel flags related to memory management
    typedef enum {
        NO_DELETE = 0,
        DELETE_ON_CANCEL = 1
    } cancel_flags_t;

    Timer(cancel_flags_t cancel_flags = DELETE_ON_CANCEL)
        : cancel_flags_(cancel_flags),
          pending_(false),
          cancelled_(false)
    {}
    
    virtual ~Timer() 
    {
        /*
         * The only time a timer should be deleted is after it fires,
         * so assert as such.
         */
        ASSERTF(pending_ == false, "can't delete a pending timer");
    }
    
    virtual cancel_flags_t cancel_flags()   { return cancel_flags_; }

    virtual void schedule_at(struct timeval *when);
    virtual void schedule_at(const Time& when);

    virtual void schedule_in(int milliseconds);

    virtual void schedule_immediate();

    virtual bool cancel();
    virtual void set_cancelled()    { cancelled_ = true; }
    virtual void clear_cancelled()  { cancelled_ = false; }
    virtual bool cancelled() { return cancelled_; }

    virtual void set_pending()   { pending_ = true; }
    virtual void clear_pending() { pending_ = false; }
    virtual bool pending() { return pending_; }

    virtual void set_when()                  { ::gettimeofday(&when_, 0); }
    virtual void set_when(struct timeval* t) { when_ = *t; }
    virtual struct timeval when() { return when_; }

    virtual void       set_seqno(u_int32_t t)  { seqno_ = t; }
    virtual u_int32_t  seqno()                 { return seqno_; }
    
    virtual void timeout(const struct timeval& now) = 0;

protected:
//    friend class TimerSystem;
//    friend class TimerCompare;

    struct timeval when_;	  ///< When the timer should fire
    cancel_flags_t cancel_flags_; ///< Should we keep the timer around
                                  ///< or delete it when the cancelled
                                  ///< timer bubbles to the top
    u_int32_t      seqno_;        ///< seqno used to break ties

private:
    bool           pending_;	  ///< Is the timer currently pending
    bool           cancelled_;	  ///< Is this timer cancelled
};

/**
 * The Timer comparator function used in the priority queue.
 */
bool
TimerCompare::operator()(Timer* a, Timer* b)
{
    if (TIMEVAL_GT(a->when(), b->when())) return true;
    if (TIMEVAL_LT(a->when(), b->when())) return false;
    return a->seqno() > b->seqno();
}



/**
 * SafeTimerState used with oasys::Ref provides a smart pointer means of 
 * controlling a [Safe]Timer to avoid timing issues of trying to cancel 
 * a timer at the same time it is  executing and deleting itself. 
 */
class SafeTimer;

class SafeTimerState {
public:
    /**
     * Constructor that takes the debug logging path to be used for
     * add and delete reference logging.
     */
    SafeTimerState(SafeTimer* parent);

    /**
     * Virtual destructor declaration.
     */
    virtual ~SafeTimerState();

    /**
     * Cancel the timer
     */
    virtual bool cancel();
    virtual void clear_cancelled();
    virtual bool set_cancelled();
    virtual bool cancelled();

    /**
     * Set the pending state
     */
    virtual void clear_pending();
    virtual void set_pending();
    virtual bool pending();

    /**
     * Bump up the reference count.
     *
     * @param what1 debugging string identifying who is incrementing
     *              the refcount
     * @param what2 optional additional debugging info
     */
    void add_ref(const char* what1, const char* what2 = "") const;

    /**
     * Bump down the reference count.
     *
     * @param what1 debugging string identifying who is decrementing
     *              the refcount
     * @param what2 optional additional debugging info
     */
    void del_ref(const char* what1, const char* what2 = "") const;

    /**
     * Hook called when the refcount goes to zero.
     */
    virtual void no_more_refs() const;

    /**
     * Accessor for the refcount value.
     */
    u_int32_t refcount() const { return refcount_.value; }

protected:
    /// The reference count
    mutable atomic_t refcount_;

    SafeTimer*     parent_;       ///< pointer back to the parent timer

    bool           pending_;	  ///< Is the timer currently pending
    bool           cancelled_;	  ///< Is this timer cancelled
    bool           processing_cancel_request_;	  ///< prevent deadly embrace

    SpinLock       lock_;         ///< serialize access
};


typedef oasys::Ref<SafeTimerState> SafeTimerStateRef;


class SafeTimer: public Timer {
public:
    SafeTimer();

    virtual ~SafeTimer();

    virtual SafeTimerStateRef get_state();





    virtual bool cancel();
    virtual void set_cancelled();
    virtual void clear_cancelled();
    virtual bool cancelled();

    virtual void set_pending();
    virtual void clear_pending();
    virtual bool pending();



protected:
    virtual void init();

protected:

    SafeTimerStateRef state_ref_;

};











/**
 * For use with the QueuingTimer, this struct defines a TimerEvent,
 * i.e. a particular firing of a Timer that captures the timer and the
 * time when it fired.
 */
struct TimerEvent {
    TimerEvent(const Timer* timer, struct timeval* time)
        : timer_(timer), time_(*time)
    {
    }
    
    const Timer* timer_;
    struct timeval time_;
};

/**
 * The queue type used in the QueueingTimer.
 */
typedef MsgQueue<TimerEvent> TimerEventQueue;
    
/**
 * A Timer class that's useful in cases when a separate thread (i.e.
 * not the main TimerSystem thread) needs to process the timer event.
 * Note that multiple QueuingTimer instances can safely share the same
 * event queue.
 */
class QueuingTimer : public Timer {
public:
    QueuingTimer(TimerEventQueue* queue) : queue_(queue) {}
    
    virtual void timeout(const struct timeval& now)
    {
        queue_->push(TimerEvent(this, (struct timeval*) &now));
    }

    virtual void timeout(struct timeval* now)
    {
        queue_->push(TimerEvent(this, now));
    }
    
protected:
    TimerEventQueue* queue_;
};

} // namespace oasys

#endif /* OASYS_TIMER_H */

