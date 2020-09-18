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

#include <cstdio>
#include <sys/time.h>

#include "debug/Log.h"
#include <thread/SpinLock.h>
#include <thread/Thread.h>
#include <util/UnitTest.h>

#ifndef __NO_ATOMIC__

using namespace oasys;

SpinLock slock;
SpinLock lock2;
volatile int nspins = 0;
volatile int total  = 0;
volatile int count1 = 0;
volatile int count2 = 0;

class Thread1 : public Thread {
public:
    Thread1(SpinLock* l) : Thread("Thread1", CREATE_JOINABLE), lock_(l) {}
    
protected:
    virtual void run() {
        for (int i = 0; i < nspins; ++i) {
            if (lock_)
                lock_->lock("Thread1");
            ++count1;
            ++total;
            if (lock_)
                lock_->unlock();
        }
    }

    SpinLock* lock_;
};

class Thread2 : public Thread {
public:
    Thread2(SpinLock* l) : Thread("Thread2", CREATE_JOINABLE), lock_(l) {}
    
protected:
    virtual void run() {
        for (int i = 0; i < nspins; ++i) {
            if (lock_)
                lock_->lock("Thread2");
            
            ++count2;
            ++total;
            if (lock_)
                lock_->unlock();
        }
    }

    SpinLock* lock_;
};

int
test(const char* what, SpinLock* lock1, SpinLock* lock2, int n)
{
    (void)what;
    
    nspins = n;
             
    Thread* t1 = new Thread1(lock1);
    Thread* t2 = new Thread2(lock2);

    t1->start();
    t2->start();

    while (count1 != nspins && count2 != nspins) {
        log_notice_p("/test", "count1:     %d", count1);
        log_notice_p("/test", "count2:     %d", count2);
        sleep(1);
    }

    t1->join();
    t2->join();

#ifndef NDEBUG
    log_notice_p("/log", "total spins: %d",  SpinLock::total_spins_.value);
    log_notice_p("/log", "total yields: %d", SpinLock::total_yields_.value);
#endif

    log_notice_p("/test", "count1:     %d", count1);
    log_notice_p("/test", "count2:     %d", count2);
    log_notice_p("/test", "total:      %d", total);
    log_notice_p("/test", "count sum:  %d", count1 + count2);
    delete t1;
    delete t2;

    count1 = 0;
    count2 = 0;
    total = 0;

#ifndef NDEBUG
    SpinLock::total_spins_ = 0;
    SpinLock::total_yields_ = 0;
#endif

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SharedLockQuick) {
    return test("shared", &slock, &slock, 100);
}

DECLARE_TEST(SharedLock) {
    return test("shared", &slock, &slock, 10000000);
}

DECLARE_TEST(SeparateLock) {
    return test("shared", &slock, &lock2, 10000000);
}
    
DECLARE_TEST(NoLock) {
    return test("shared", NULL, NULL, 10000000);
}

DECLARE_TESTER(SpinLockTester) {
    ADD_TEST(SharedLockQuick);
    ADD_TEST(SharedLock);
    ADD_TEST(SeparateLock);
    ADD_TEST(NoLock);
}

#else // __NO_ATOMIC__

DECLARE_TEST(Bogus) {
    log_warn("spin lock test is meaningless without atomic.h");
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(SpinLockTester) {
    ADD_TEST(Bogus);
}

#endif // __NO_ATOMIC__

DECLARE_TEST_FILE(SpinLockTester, "spin lock test");
