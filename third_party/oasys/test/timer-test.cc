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

#include <stdio.h>
#include <unistd.h>
#include <thread/SharedTimer.h>

#include "debug/Log.h"
#include "util/UnitTest.h"

using namespace oasys;

class OneShotTimer : public SharedTimer {
public:
    OneShotTimer(bool quiet = false) : quiet_(quiet), fired_(false) {}
    virtual ~OneShotTimer() { printf("OneShotTimer destroyed\n"); fflush(stdout); }
    void timeout(const struct timeval& now) {
        if (! quiet_) {
            log_notice_p("/timer/oneshot", "fired at %ld.%ld",
                         (long unsigned int)now.tv_sec,
                         (long unsigned int)now.tv_usec);
        }
        
        fired_ = true;
    }
    bool quiet_;
    volatile bool fired_;
};


class PeriodicTimer : public SharedTimer {
  public:
    PeriodicTimer(const char* name) {
        snprintf(log_, sizeof(log_), "/timer/%s", name);
        logf(log_, LOG_DEBUG, "PeriodicTimer %p", this);
        count_ = 0;
    }
    virtual ~PeriodicTimer() { printf("PeriodicTimer destroyed\n"); fflush(stdout); }
    
    void timeout(const struct timeval& now) {
        int late = TIMEVAL_DIFF_USEC(now, when());
        log_notice_p(log_, "timer at %ld.%ld (%d usec late)",
                     (long unsigned int)now.tv_sec, (long unsigned int)now.tv_usec,
                     late);
        ++count_;
        reschedule();
    }

    virtual void set_timer_sptr(SPtr_Timer sptr) { sptr_ = sptr; }
    virtual void clear_set_timer_sptr() { sptr_ = nullptr; }

    virtual void reschedule() = 0;
    virtual void cancel() 
    { 
        SharedTimer::cancel(sptr_); 
        // release the internal reference to this timer
        sptr_ = nullptr;
    }
    
    int count_;
    
  protected:
    char log_[64];
    SPtr_Timer sptr_;
};

class OneSecondTimer : public PeriodicTimer {
  public:
    OneSecondTimer() : PeriodicTimer("1sec") {}
    virtual ~OneSecondTimer() { printf("OneSecondTimer destroyed\n"); fflush(stdout); }
    void reschedule() { schedule_in(1000, sptr_); }
};
               
class TenSecondTimer : public PeriodicTimer {
  public:
    TenSecondTimer() : PeriodicTimer("10sec") {}
    virtual ~TenSecondTimer() { printf("TenSecondTimer destroyed\n"); fflush(stdout); }
    void reschedule() { schedule_in(10000, sptr_); }
};
               
class HalfSecondTimer : public PeriodicTimer {
  public:
    HalfSecondTimer() : PeriodicTimer("500msec") {}
    virtual ~HalfSecondTimer() { printf("HalfSecondTimer destroyed\n"); fflush(stdout); }
    void reschedule() { schedule_in(500, sptr_); }
};

class TenImmediateTimer : public PeriodicTimer {
public:
    TenImmediateTimer() : PeriodicTimer("10immed")
    {
        count_ = 0;
    }
    virtual ~TenImmediateTimer() { printf("TenImmediateTimer destroyed\n"); fflush(stdout); }
    
    void reschedule() {
        if (count_ == 0 || (count_ % 10 != 0)) {
            schedule_in(1, sptr_);
        } else {
            schedule_in(1000, sptr_);
        }
    }
};

class ConcurrentTimer;
typedef std::shared_ptr<ConcurrentTimer> SPtr_ConcurrentTimer;

class ConcurrentTimer : public SharedTimer {
public:
    ConcurrentTimer(std::vector<SPtr_ConcurrentTimer>* completed)
        : completed_(completed) {}
    virtual ~ConcurrentTimer() { printf("ConcurrentTimer destroyed\n"); fflush(stdout); }

    virtual void set_timer_sptr(SPtr_ConcurrentTimer sptr) { sptr_ = sptr; }

    void timeout(const timeval& now) {
        (void)now;
        completed_->push_back(sptr_);
        sptr_ = nullptr;
    }
    
    std::vector<SPtr_ConcurrentTimer>* completed_;
    SPtr_ConcurrentTimer sptr_;
};


typedef std::shared_ptr<OneShotTimer> SPtr_OneShotTimer;
typedef std::shared_ptr<PeriodicTimer> SPtr_PeriodicTimer;


DECLARE_TEST(Init) {
    SPtr_OneShotTimer startup_sptr = std::make_shared<OneShotTimer>();
    SPtr_Timer sptr = startup_sptr;

    SharedTimerSystem::create();
    SharedTimerThread::init();
    startup_sptr->schedule_in(10, sptr);
    while (! startup_sptr->fired_) {
        usleep(100);
    }
    
    return UNIT_TEST_PASSED;
}



DECLARE_TEST(OneSec) {
    SPtr_PeriodicTimer t = std::make_shared<OneSecondTimer>();
    SPtr_Timer sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();

    usleep(5500000);
    t->cancel();
    sleep(1);
    CHECK_EQUAL(t->count_, 5);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TenSec) {
    SPtr_PeriodicTimer t = std::make_shared<TenSecondTimer>();
    SPtr_Timer sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();

    sleep(35);
    t->cancel();
    CHECK_EQUAL(t->count_, 3);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(HalfSec) {
    SPtr_PeriodicTimer t = std::make_shared<HalfSecondTimer>();
    SPtr_Timer sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();

    usleep(5250000);
    t->cancel();
    sleep(1);
    CHECK_EQUAL(t->count_, 10);
    return UNIT_TEST_PASSED;
}



DECLARE_TEST(Simultaneous) {
    std::vector<SPtr_PeriodicTimer> timers;

    SPtr_PeriodicTimer t = std::make_shared<OneSecondTimer>();
    SPtr_Timer sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = std::make_shared<TenSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = std::make_shared<OneSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = std::make_shared<HalfSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    usleep(500000);
    t = std::make_shared<OneSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = std::make_shared<HalfSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    usleep(100000);
    t = std::make_shared<HalfSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    usleep(100000);
    t = std::make_shared<HalfSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    usleep(100000);
    t = std::make_shared<HalfSecondTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = std::make_shared<TenImmediateTimer>();
    sptr = t;
    t->set_timer_sptr(sptr);
    t->reschedule();
    timers.push_back(t);

    t = nullptr;
    sptr = nullptr;

    sleep(10);

    for (u_int i = 0; i < timers.size(); ++i) {
        timers[i]->cancel();
    }
    timers.clear();

    printf("Simultaneous timers_vec cleared\n");
    fflush(stdout);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Many) {
    std::vector<SPtr_OneShotTimer> timers;
    int n = 500;
    int m = 50;
    log_notice_p("/test", "posting %d timers (in batches of %d)", n, m);
    for (int i = 0; i < (n / m); ++i) {
        // do 50 at a time, waiting 1 second between batches
        for (int j = 0; j < m; ++j) {
            SPtr_OneShotTimer ost_sptr = std::make_shared<OneShotTimer>((j != 0) ? true : false);
            SPtr_Timer sptr = ost_sptr;
            ost_sptr->schedule_in(10000, sptr);
            timers.push_back(ost_sptr);
        }
        sleep(1);
    }
    log_notice_p("/test", "waiting for timers to fire (1st of each batch will log)");

    sleep(15);

    for (int i = 0; i < n; ++i) {
        //timers[i]->cancel();
        SPtr_Timer sptr = timers[i];
        SharedTimerSystem::instance()->cancel(sptr);

        if (! timers[i]->fired_) {
            log_err_p("/test", "timer %d never fired!!", i);
        }
    }
    timers.clear();
    printf("Many timers_vec cleared\n");
    fflush(stdout);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Concurrent) {
    struct timeval when;
    ::gettimeofday(&when, 0);
    when.tv_sec += 5;

    int N = 100;

    std::vector<SPtr_ConcurrentTimer> timers_, completed_;
    for (int i = 0; i < N; ++i) {
        SPtr_ConcurrentTimer t = std::make_shared<ConcurrentTimer>(&completed_);
        t->set_timer_sptr(t);

        timers_.push_back(t);

        SPtr_Timer sptr = t;
        t->schedule_at(&when, sptr);
    }

    sleep(6);
    
    CHECK_EQUAL(timers_.size(), completed_.size());
    for (int i = 0; i < N; ++i) {
        CHECK(timers_[i] == completed_[i]);
    }
    timers_.clear();
    completed_.clear();

    printf("Concurrent timers_vec cleared\n");
    fflush(stdout);
    return UNIT_TEST_PASSED;
}


DECLARE_TEST(Sleep) {
    printf("Sleeping 10 seconds to allow shared_ptr deletes - pending timers: %zu\n",
           oasys::SharedTimerSystem::instance()->num_pending_timers());

    fflush(stdout);
    sleep(10);
    return UNIT_TEST_PASSED;
}



DECLARE_TESTER(TimerTest) {
    ADD_TEST(Init);
    ADD_TEST(OneSec);
    ADD_TEST(TenSec);
    ADD_TEST(HalfSec);
    ADD_TEST(Simultaneous);
    ADD_TEST(Many);
    ADD_TEST(Concurrent);
    ADD_TEST(Sleep);
}

DECLARE_TEST_FILE(TimerTest, "timer test");

#ifdef DECLARE_TEST_TCL
proc checkTimerTest1 {output} {
    while {[set line [gets $output]] != ""} {
        if [regexp {\(-?([0-9]+) usec late\)} $line match diff] {
            # check for alarms that are off by 20 msec.
            if [expr $diff > 20000] {
                puts "Timer off by more than 20000 usec"
                return -1
            }
        }
    }

    return 0
}
#endif

