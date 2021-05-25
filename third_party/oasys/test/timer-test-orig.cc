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
#include <thread/Timer.h>

#include "debug/Log.h"
#include "util/UnitTest.h"

using namespace oasys;

class OneShotTimer : public Timer {
public:
    OneShotTimer(bool quiet = false) : quiet_(quiet), fired_(false) {}
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

class PeriodicTimer : public Timer {
  public:
    PeriodicTimer(const char* name) {
        snprintf(log_, sizeof(log_), "/timer/%s", name);
        logf(log_, LOG_DEBUG, "PeriodicTimer %p", this);
        count_ = 0;
    }
    
    void timeout(const struct timeval& now) {
        int late = TIMEVAL_DIFF_USEC(now, when());
        log_notice_p(log_, "timer at %ld.%ld (%d usec late)",
                     (long unsigned int)now.tv_sec, (long unsigned int)now.tv_usec,
                     late);
        ++count_;
        reschedule();
    }
    
    virtual void reschedule() = 0;
    
    int count_;
    
  protected:
    char log_[64];
};

class TenSecondTimer : public PeriodicTimer {
  public:
    TenSecondTimer() : PeriodicTimer("10sec") { reschedule(); }
    void reschedule() { schedule_in(10000); }
};
               
class OneSecondTimer : public PeriodicTimer {
  public:
    OneSecondTimer() : PeriodicTimer("1sec") { reschedule(); }
    void reschedule() { schedule_in(1000); }
};
               
class HalfSecondTimer : public PeriodicTimer {
  public:
    HalfSecondTimer() : PeriodicTimer("500msec") { reschedule(); }
    void reschedule() { schedule_in(500); }
};

class TenImmediateTimer : public PeriodicTimer {
public:
    TenImmediateTimer() : PeriodicTimer("10immed")
    {
        count_ = 0;
        reschedule();
    }
    
    void reschedule() {
        if (count_ == 0 || (count_ % 10 != 0)) {
            schedule_in(1);
        } else {
            schedule_in(1000);
        }
    }
};

class ConcurrentTimer : public Timer {
public:
    ConcurrentTimer(std::vector<ConcurrentTimer*>* completed)
        : completed_(completed) {}

    void timeout(const timeval& now) {
        (void)now;
        completed_->push_back(this);
    }
    
    std::vector<ConcurrentTimer*>* completed_;
};

DECLARE_TEST(Init) {
    OneShotTimer startup;
    TimerSystem::create();
    TimerThread::init();
    startup.schedule_in(10);
    while (! startup.fired_) {
        usleep(100);
    }
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TenSec) {
    PeriodicTimer* t = new TenSecondTimer();
    sleep(35);
    t->cancel();
    CHECK_EQUAL(t->count_, 3);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(OneSec) {
    PeriodicTimer* t = new OneSecondTimer();
    usleep(5500000);
    t->cancel();
    sleep(1);
    CHECK_EQUAL(t->count_, 5);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(HalfSec) {
    PeriodicTimer* t = new HalfSecondTimer();
    usleep(5250000);
    t->cancel();
    sleep(1);
    CHECK_EQUAL(t->count_, 10);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Simultaneous) {
    std::vector<Timer*> timers;

    timers.push_back(new OneSecondTimer());

    timers.push_back(new TenSecondTimer());
    timers.push_back(new OneSecondTimer());
    timers.push_back(new HalfSecondTimer());

    usleep(500000);
    timers.push_back(new OneSecondTimer());

    timers.push_back(new HalfSecondTimer());
    usleep(100000);
    timers.push_back(new HalfSecondTimer());
    usleep(100000);
    timers.push_back(new HalfSecondTimer());
    usleep(100000);
    timers.push_back(new HalfSecondTimer());

    timers.push_back(new TenImmediateTimer());

    sleep(10);

    for (u_int i = 0; i < timers.size(); ++i) {
        timers[i]->cancel();
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Many) {
    std::vector<OneShotTimer*> timers;
    int n = 500;
    int m = 50;
    log_notice_p("/test", "posting %d timers (in batches of %d)", n, m);
    for (int i = 0; i < (n / m); ++i) {
        // do 50 at a time, waiting 1 second between batches
        for (int j = 0; j < m; ++j) {
            OneShotTimer* t = new OneShotTimer((j != 0) ? true : false);
            t->schedule_in(10000);
            timers.push_back(t);
        }
        sleep(1);
    }
    log_notice_p("/test", "waiting for timers to fire (1st of each batch will log)");

    sleep(15);

    for (int i = 0; i < n; ++i) {
        timers[i]->cancel();

        if (! timers[i]->fired_) {
            log_err_p("/test", "timer %d never fired!!", i);
        }
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Concurrent) {
    struct timeval when;
    ::gettimeofday(&when, 0);
    when.tv_sec += 5;

    int N = 100;

    std::vector<ConcurrentTimer*> timers_, completed_;
    for (int i = 0; i < N; ++i) {
        ConcurrentTimer* t = new ConcurrentTimer(&completed_);
        timers_.push_back(t);
        t->schedule_at(&when);
    }

    sleep(6);
    
    CHECK_EQUAL(timers_.size(), completed_.size());
    for (int i = 0; i < N; ++i) {
        CHECK(timers_[i] == completed_[i]);
    }
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

