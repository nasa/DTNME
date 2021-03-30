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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <unistd.h>
#include <sys/time.h>
#include <sys/poll.h>

#include "Timer.h"
#include "io/IO.h"
#include "../util/InitSequencer.h"

namespace oasys {

template <> SharedTimerSystem* Singleton<SharedTimerSystem>::instance_ = 0;

//----------------------------------------------------------------------
SharedTimerSystem::SharedTimerSystem()
    : Logger("SharedTimerSystem", "/timer"),
      system_lock_(new SpinLock()),
      cancel_lock_(new SpinLock()),
      notifier_(logpath_),
      timer_q1_(),
      timer_q2_(),
      seqno_(0),
      num_cancelled_(0),
      old_num_cancelled_(0)
{
    memset(handlers_, 0, sizeof(handlers_));
    memset(signals_, 0, sizeof(signals_));
    sigfired_ = false;

    timers_ = &timer_q1_;
    old_timers_ = &timer_q2_;

    should_stop_ = false;
    pause_processing_ = false;
}

//----------------------------------------------------------------------
SharedTimerSystem::~SharedTimerSystem()
{
    while (! timers_->empty()) {
        SPtr_Timer t = timers_->top();
        t->clear_pending(); // to avoid assertion
        timers_->pop();
    }

    while (! old_timers_->empty()) {
        SPtr_Timer t = old_timers_->top();
        t->clear_pending(); // to avoid assertion
        old_timers_->pop();
    }
    
    delete cancel_lock_;
    delete system_lock_;

    instance_ = NULL;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::shutdown()
{
    ScopeLock cl(system_lock_, "SharedTimerSystem::shutdown");
    should_stop_ = true;
    notifier_.signal();
}

//----------------------------------------------------------------------
void
SharedTimerSystem::pause_processing()
{
    pause_processing_ = true;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::resume_processing()
{
    pause_processing_ = false;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::schedule_at(struct timeval *when, SPtr_Timer& timer)
{
    if (timer == nullptr) return;

    struct timeval now;
    
    if (when == 0) {
        // special case a NULL timeval as an immediate timer
        timer->set_when();
    } else {
        ::gettimeofday(&now, 0);
        timer->set_when( when );
    }
    
    if (timer->pending()) {
        // XXX/demmer this could scan through the heap, find the right
        // timer and re-sort the heap, but it seems better to just
        // expose a new "reschedule" api call to make it explicit that
        // it's a different operation.
        PANIC("rescheduling timers not implemented");
    }
    
    timer->set_pending();
    timer->clear_cancelled();
    timer->set_seqno( seqno_++ );

    ScopeLock l(system_lock_, "SharedTimerSystem::schedule_at");
    if (!should_stop_) {    
        timers_->push(timer);
    } else {
        timer->clear_pending();
    }

    notifier_.signal();
}

//----------------------------------------------------------------------
void
SharedTimerSystem::schedule_in(int milliseconds, SPtr_Timer& timer)
{
    if (timer == nullptr) return;

    struct timeval when;
    ::gettimeofday(&when, 0);
    when.tv_sec += milliseconds / 1000;
    when.tv_usec += (milliseconds % 1000) * 1000;
    while (when.tv_usec > 1000000) {
        when.tv_sec += 1;
        when.tv_usec -= 1000000;
    }
    
    return schedule_at(&when, timer);
}

//----------------------------------------------------------------------
void
SharedTimerSystem::schedule_immediate(SPtr_Timer& timer)
{
    if (timer == nullptr) return;

    return schedule_at(0, timer);
}

//----------------------------------------------------------------------
bool
SharedTimerSystem::cancel(SPtr_Timer& timer)
{
    if (timer == nullptr) return false;

    if (!should_stop_ && !pause_processing_) {
        ScopeLock cl(cancel_lock_, "SharedTimerSystem::cancel");

        //XXX/dz in case timer pop on this timer is in progress
        timer->set_cancelled();

        // There's no good way to get a timer out of a heap, so we let it
        // stay in there and mark it as cancelled so when it bubbles to
        // the top, we don't bother with it. This makes rescheduling a
        // single timer instance tricky...
        if (timer->pending()) {
            num_cancelled_++;
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
size_t
SharedTimerSystem::num_pending_timers()
{
    ScopeLock cl(cancel_lock_, "SharedTimerSystem::num_pending_timers");

    return timer_q1_.size() + timer_q2_.size();
}

//----------------------------------------------------------------------
void
SharedTimerSystem::post_signal(int sig)
{
    SharedTimerSystem* _this = SharedTimerSystem::instance();

    _this->sigfired_ = true;
    _this->signals_[sig] = true;
    
    _this->notifier_.signal();
}

//----------------------------------------------------------------------
void
SharedTimerSystem::add_sighandler(int sig, sighandlerfn_t* handler)
{
    log_debug("adding signal handler %p for signal %d", handler, sig);
    handlers_[sig] = handler;
    signal(sig, post_signal);
}

//----------------------------------------------------------------------
void
SharedTimerSystem::process_popped_timer(const struct timeval& now, SPtr_Timer& next_timer)
{
    if (! next_timer->cancelled()) {
        int late = TIMEVAL_DIFF_MSEC(now, next_timer->when());
        if (late > 2000) {
            log_warn("timer thread running slow -- timer is %d msecs late", late);
        }
        
        if (!should_stop_) {
            next_timer->timeout(now);
        }
    } else {
        if (num_cancelled_ > 0) {
            num_cancelled_--;
        } else {
            //log_warn("SharedTimerSystem::process_popped_timer - num_cancelled_ timers is zero while popping cancelled timer");
        }
    }
}

//----------------------------------------------------------------------
void
SharedTimerSystem::handle_signals()
{        
    // KNOWN ISSUE: if a second signal is received before the first is
    // handled it is ignored, i.e. sending signal gives at-least-once
    // semantics, not exactly-once semantics
    if (sigfired_) {
        sigfired_ = 0;
        
        //log_debug("sigfired_ set, calling registered handlers");
        for (int i = 0; i < NSIG; ++i) {
            if (signals_[i]) {
                // XXX/dz caught a segfault trying to process sig 0??
                if (handlers_[i]) {
                    handlers_[i](i);
                }
                signals_[i] = 0;
            }
        }
    }
}

//----------------------------------------------------------------------
int
SharedTimerSystem::run_expired_timers()
{
   int time_to_next_expiration = -1;


    struct timeval now;
    SPtr_Timer next_timer;
    int diff_ms;

    // minimizing the time that the lock is held
    // so that new timers do not get delayed excessively        
    //ScopeLock l(system_lock_, "TimerSystem::run_expired_timers");
    system_lock_->lock("SharedTimerSystem::run_expired_timers - signals");
    
    handle_signals();

    while ( !should_stop_ && !pause_processing_ &&
            (-1 == time_to_next_expiration) && !timers_->empty() ) 
    {
        // do not allow cancel while working with this next timer
        cancel_lock_->lock("SharedTimerSystem::run_expired_timers get top");


        next_timer = timers_->top();

        if (next_timer != nullptr) {
            if (next_timer->cancelled()) {
                // if the next timer is cancelled, pop it immediately,
                // regardless of whether it's time has come or not
                // and fall through to "process" it
            } else {
                if (::gettimeofday(&now, 0) != 0) {
                    PANIC("gettimeofday");
                }

                if (TIMEVAL_LT(now, next_timer->when())) {

                    // XXX/demmer it's possible that the next timer is too far
                    // in the future to be expressable in milliseconds, so we
                    // just max it out
                    if (next_timer->when().tv_sec - now.tv_sec < (INT_MAX / 1000)) {
                        diff_ms = TIMEVAL_DIFF_MSEC(next_timer->when(), now);
                    } else {
                        //log_debug("diff millisecond overflow: "
                        //          "next timer due at %u.%u, now %u.%u",
                        //          (u_int)next_timer->when().tv_sec,
                        //          (u_int)next_timer->when().tv_usec,
                        //          (u_int)now.tv_sec,
                        //          (u_int)now.tv_usec);
                
                        diff_ms = INT_MAX;
                    }

                    ASSERTF(diff_ms >= 0,
                            "next timer due at %u.%u, now %u.%u, diff %d",
                            (u_int)next_timer->when().tv_sec,
                            (u_int)next_timer->when().tv_usec,
                            (u_int)now.tv_sec,
                            (u_int)now.tv_usec,
                            diff_ms);
            
                    // there's a chance that we're within a millisecond of the
                    // time to pop, but still not at the right time. in this
                    // case case we don't return 0, but fall through to pop
                    // the timer after adjusting the "current time"
                    if (diff_ms == 0) {
                        //log_debug("sub-millisecond difference found, falling through");
                        now = next_timer->when();
                    } else {
                        // clear the next_timer pointer so it does not get processed
                        next_timer = nullptr;
                        time_to_next_expiration = diff_ms;
                    }
                }
            }
        }


        // verify the top timer is still the same one
        if (next_timer == nullptr) {
            cancel_lock_->unlock();
        } else {
            timers_->pop();

            // clear the pending bit since it could get rescheduled 
            ASSERT(next_timer->pending());
            next_timer->clear_pending();

            // release the locks while processing the timer
            cancel_lock_->unlock();
            system_lock_->unlock();

            process_popped_timer(now, next_timer);

            next_timer = nullptr;

            // reaquire the lock for the next pass through the loop
            system_lock_->lock("SharedTimerSystem::run_expired_timers - after pop");
        }
    }

    system_lock_->unlock();

    return time_to_next_expiration;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::reinsert_timer(SPtr_Timer& timer)
{
    ScopeLock l(system_lock_, "SharedTimerSystem::reinsert_timer");

    timers_->push(timer);

    notifier_.signal();
}

//----------------------------------------------------------------------
void
SharedTimerSystem::process_old_cancelled_timer(SPtr_Timer& timer, 
                                         const struct timeval& now)
{
    (void) now;
    timer->clear_pending();
    old_num_cancelled_--;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::check_cancelled_timers()
{
    SPtr_Timer next_timer;
    //bool found_good_timer = false;
    struct timeval now;    

    if (old_timers_->empty()) {
        // have canceled timers accumulated enough to warrant cleaning up?

        cancel_lock_->lock("SharedTimerSystem::check_cancelled_timers - calcs");
        size_t active_timers = timers_->size() - num_cancelled_;
        size_t cancelled_timers = num_cancelled_;
        cancel_lock_->unlock();

        if ((cancelled_timers >= active_timers) || (cancelled_timers >= 1000)) {
            system_lock_->lock("SharedTimerSystem::check_cancelled_timers - swap timer queues");
            cancel_lock_->lock("SharedTimerSystem::check_cancelled_timers - swap timer queues");

            // swap the timer queues
            if (timers_ == &timer_q1_) {
                timers_ = &timer_q2_;
                old_timers_ = &timer_q1_;
            } else {
                timers_ = &timer_q1_;
                old_timers_ = &timer_q2_;
            }
            old_num_cancelled_ = num_cancelled_;
            num_cancelled_ = 0;

            // minimizing the time that the lock is held
            // so that new timers do not get delayed excessively        
            cancel_lock_->unlock();
            system_lock_->unlock();
        }
    }

    // process all of the old timers at our leisure since 
    // we are not locking out the TimerSystem
    int ctr = 0;
    while (! old_timers_->empty()) 
    {
        next_timer = old_timers_->top();
        old_timers_->pop();

        if (! next_timer->cancelled()) {
            reinsert_timer(next_timer);
        } else {
            if (::gettimeofday(&now, 0) != 0) {
                PANIC("gettimeofday");
            }

            process_old_cancelled_timer(next_timer, now);
        }
        next_timer = nullptr;

        if (++ctr >= 100) {
            ctr = 0;
            usleep(1);
        }
    }

    // it is possible that some timers were cancelled while
    // they were in old_timers_ queue so just zero out the
    // old_num_cancelled_ count and it should all come out
    // in the wash eventually
    old_num_cancelled_ = 0;
}

//----------------------------------------------------------------------
void
SharedTimerSystem::cancel_all_timers()
{
    SPtr_Timer next_timer;

    int num_cancels = 0;
    int num_deleted = 0;

    system_lock_->lock("SharedTimerSystem::cancel_all_timers");
    cancel_lock_->lock("SharedTimerSystem::cancel_all_timers");

    while (! timer_q1_.empty()) 
    {
        next_timer = timer_q1_.top();
        timer_q1_.pop();

        next_timer->set_cancelled();
        next_timer->clear_pending();

        ++num_cancels;
        
        if (next_timer->cancel_flags() == SharedTimer::DELETE_ON_CANCEL) {
            ++num_deleted;
        }
    }

    while (! timer_q2_.empty()) 
    {
        next_timer = timer_q2_.top();
        timer_q2_.pop();

        next_timer->set_cancelled();
        next_timer->clear_pending();
        
        ++num_cancels;
        
        if (next_timer->cancel_flags() == SharedTimer::DELETE_ON_CANCEL) {
            ++num_deleted;
        }
    }

    old_num_cancelled_ = 0;
    cancel_lock_->unlock();
    system_lock_->unlock();

    if (!destroy_singletons_on_exit_  && !getenv("OASYS_CLEANUP_SINGLETONS")) {
        delete this;
    }
}

//----------------------------------------------------------------------
void
SharedTimerThread::run()
{
    char threadname[16] = "TimerThread";
    pthread_setname_np(pthread_self(), threadname);
   
    SharedTimerSystem* sys = SharedTimerSystem::instance();
    while (!should_stop()) 
    {
        if (pause_processing_) {
            usleep(10000);
        } else {
            int timeout = sys->run_expired_timers();
            if (!should_stop() && (sys->notifier()->wait(NULL, timeout))) {
                // if notifier was active then we need to clear it
                sys->notifier()->clear();
            }
        }
    }

    sys->cancel_all_timers();
    
    instance_ = nullptr;
    //NOTREACHED;
}

//----------------------------------------------------------------------
void
SharedTimerThread::shutdown()
{
    SharedTimerSystem* sys = SharedTimerSystem::instance();
    sys->shutdown();

    set_should_stop();  // deletes on exit
    while (instance_ != nullptr) {
        usleep(100000);
    }

    SharedTimerSystem::reset();
}

//----------------------------------------------------------------------
void
SharedTimerThread::pause_processing()
{
    pause_processing_ = true;
    SharedTimerSystem* sys = SharedTimerSystem::instance();
    sys->pause_processing();
}

//----------------------------------------------------------------------
void
SharedTimerThread::resume_processing()
{
    pause_processing_ = false;
    SharedTimerSystem* sys = SharedTimerSystem::instance();
    sys->resume_processing();
}

//----------------------------------------------------------------------
void
SharedTimerThread::init()
{
    ASSERT(instance_ == NULL);
    instance_ = new SharedTimerThread();
    instance_->start();
}

SharedTimerThread* SharedTimerThread::instance_ = NULL;

//----------------------------------------------------------------------
void
SharedTimerReaperThread::run()
{
    char threadname[16] = "TimerReaper";
    pthread_setname_np(pthread_self(), threadname);
   

    int ctr = 0;
    SharedTimerSystem* sys = SharedTimerSystem::instance();
    while (!should_stop()) 
    {
        usleep(250000);

        // roughly every 60 seconds - make configurable?
        if (++ctr >= 60*4) {
            if (!should_stop()) {
                sys->check_cancelled_timers();
            }
            ctr = 0;
        }
    }

    instance_ = NULL;
    //NOTREACHED;
}

//----------------------------------------------------------------------
void
SharedTimerReaperThread::init()
{
    ASSERT(instance_ == NULL);
    instance_ = new SharedTimerReaperThread();
    instance_->start();
}

SharedTimerReaperThread* SharedTimerReaperThread::instance_ = NULL;





//----------------------------------------------------------------------
void
SharedTimer::schedule_at(struct timeval *when, SPtr_Timer& sptr)
{
    SharedTimerSystem::instance()->schedule_at(when, sptr);
}
    
//----------------------------------------------------------------------
void
SharedTimer::schedule_at(const Time& when, SPtr_Timer& sptr)
{
    struct timeval tv;
    tv.tv_sec  = when.sec_;
    tv.tv_usec = when.usec_;
    SharedTimerSystem::instance()->schedule_at(&tv, sptr);
}

//----------------------------------------------------------------------
void
SharedTimer::schedule_in(int milliseconds, SPtr_Timer& sptr)
{
    SharedTimerSystem::instance()->schedule_in(milliseconds, sptr);
}

//----------------------------------------------------------------------
void
SharedTimer::schedule_immediate(SPtr_Timer& sptr)
{
    SharedTimerSystem::instance()->schedule_immediate(sptr);
}

//----------------------------------------------------------------------
bool
SharedTimer::cancel(SPtr_Timer& sptr)
{
    return SharedTimerSystem::instance()->cancel(sptr);
}



} // namespace oasys
