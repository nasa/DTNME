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

#include <cstdio>
#include <sys/time.h>

#include "debug/Log.h"
#include <thread/Atomic.h>
#include <thread/Thread.h>
#include <util/UnitTest.h>

#ifndef __NO_ATOMIC__

using namespace oasys;

class AddRetThread : public Thread {
public:
    AddRetThread(atomic_t* barrier, atomic_t* val,
                 atomic_t* sum, u_int32_t count, u_int32_t amount)
        : Thread("AddRetThread", CREATE_JOINABLE),
          barrier_(barrier), val_(val), sum_(sum), count_(count), amount_(amount) {}
    
protected:
    virtual void run() {
        atomic_incr(barrier_);
        while (barrier_->value != 0) {}

        log_notice_p("/test", "thread %p starting", this);
        for (u_int i = 0; i < count_; ++i) {
            u_int32_t newval;
            if (amount_ == 1) {
                newval = atomic_incr_ret(val_);
            } else {
                newval = atomic_add_ret(val_, amount_);
            }
            atomic_add(sum_, newval);
        }
        log_notice_p("/test", "thread %p done", this);
    }

    atomic_t* barrier_;
    atomic_t* val_;
    atomic_t* sum_;
    volatile u_int32_t count_;
    volatile u_int32_t amount_;
};

class IncrThread : public Thread {
public:
    IncrThread(atomic_t* barrier, atomic_t* val,
               u_int32_t count, u_int32_t limit = 0)
        : Thread("IncrThread", CREATE_JOINABLE),
          barrier_(barrier), val_(val), count_(count), limit_(limit) {}
    
protected:
    virtual void run() {
        atomic_incr(barrier_);
        while (barrier_->value != 0) {}

        log_notice_p("/test", "thread %p starting", this);
        for (u_int i = 0; i < count_; ++i) {
            atomic_incr(val_);
        }
        log_notice_p("/test", "thread %p done", this);
    }

    atomic_t* barrier_;
    atomic_t* val_;
    volatile u_int32_t count_;
    volatile u_int32_t limit_;
};

class DecrTestThread : public Thread {
public:
    DecrTestThread(atomic_t* barrier, atomic_t* val,
                   u_int32_t count, atomic_t* nzero, u_int32_t* maxval)
        : Thread("DecrTestThread", CREATE_JOINABLE),
          barrier_(barrier), val_(val), count_(count), nzero_(nzero), maxval_(maxval) {}
    
protected:
    virtual void run() {
        atomic_incr(barrier_);
        while (barrier_->value != 0) {}
        
        log_notice_p("/test", "thread %p starting", this);

        for (u_int i = 0; i < count_; ++i) {
            while (! atomic_cmpxchg32(val_, 0, 0)) {} // wait until != 0
            
            bool zero = atomic_decr_test(val_);
            if (zero) {
                atomic_incr(nzero_);
            }

            u_int32_t cur = val_->value;
            if (cur > *maxval_) {
                *maxval_ = cur;
            }
        }
        
        log_notice_p("/test", "thread %p done", this);
    }

    atomic_t* barrier_;
    atomic_t* val_;
    volatile u_int32_t count_;
    atomic_t* nzero_;
    volatile u_int32_t* maxval_;
};

class CompareAndSwapThread : public Thread {
public:
    CompareAndSwapThread(atomic_t* barrier, atomic_t* val,
                         u_int32_t count,
                         atomic_t* success, atomic_t* fail)
        : Thread("DecrTestThread", CREATE_JOINABLE),
          barrier_(barrier), val_(val), count_(count), success_(success), fail_(fail) {}
    
protected:
    virtual void run() {
        atomic_incr(barrier_);
        while (barrier_->value != 0) {}
        
        log_notice_p("/test", "thread %p starting", this);

        for (u_int i = 0; i < count_; ++i) {
#ifdef __amd64__
            u_int32_t thisval = ((u_int64_t)this) & 0xffffffff;
#else
            u_int32_t thisval = (u_int32_t)this;
#endif
            
            u_int32_t old = atomic_cmpxchg32(val_, 0, thisval);
            if (old == 0) {
                ASSERT(val_->value == thisval);
                old = atomic_cmpxchg32(val_, thisval, 0);
                ASSERT(old == thisval);
                atomic_incr(success_);
            } else {
                atomic_incr(fail_);
            }
        }
        
        log_notice_p("/test", "thread %p done", this);
    }

    atomic_t* barrier_;
    atomic_t* val_;
    volatile u_int32_t count_;
    atomic_t* success_;
    atomic_t* fail_;
};

DECLARE_TEST(AllOps) {
    atomic_t a(0);

    CHECK(a.value == 0);
    CHECK((atomic_incr(&a), a.value == 1));
    CHECK((atomic_incr(&a), a.value == 2));
    CHECK((atomic_decr(&a), a.value == 1));
    CHECK((atomic_decr(&a), a.value == 0));

    CHECK((atomic_add(&a, 15), a.value == 15));
    CHECK((atomic_sub(&a, 5),  a.value == 10));
    CHECK((atomic_add(&a, 22), a.value == 32));
    CHECK((atomic_add(&a, 7),  a.value == 39));
    CHECK((atomic_sub(&a, 39), a.value == 0));

    CHECK((atomic_incr(&a), a.value == 1));
    CHECK((atomic_incr(&a), a.value == 2));
    CHECK(atomic_decr_test(&a) == 0);
    CHECK(atomic_decr_test(&a) == 1);
    CHECK(a.value == 0);

    CHECK((a.value = 5) == 5);
    CHECK(atomic_cmpxchg32(&a, 5, 6) == 5);
    CHECK(a.value == 6);
    CHECK(atomic_cmpxchg32(&a, 5, 6) == 6);
    CHECK(a.value == 6);
    CHECK(atomic_cmpxchg32(&a, 6, 5) == 6);
    CHECK(a.value == 5);
    
    return UNIT_TEST_PASSED;
}

int
atomic_add_ret_test(int nthreads, int count, int amount)
{
    atomic_t barrier = 0;
    atomic_t val = 0;
    atomic_t sum[nthreads];
    u_int64_t expected = 0;
    u_int64_t total_sum = 0;

    memset(sum, 0, sizeof(sum));

    Thread* threads[nthreads];

    for (int i = 0; i < nthreads; ++i) {
        threads[i] = new AddRetThread(&barrier, &val, &sum[i], count, amount);
        threads[i]->start();
    }

    while (barrier.value != (u_int)nthreads) {}
    barrier = 0;

    for (int i = 0; i < nthreads; ++i) {
        threads[i]->join();
        delete threads[i];
    }

    for (int i = 0; i < nthreads * count; ++i) {
        expected += ((i + 1) * amount);
    }

    // To make UnitTest happy, because it is usually supposed to be
    // used inside a UnitTest class
    int errno_;
    const char* strerror_;

    for (int i = 0; i < nthreads; ++i) {
        CHECK_GTU(sum[i].value, 0);
        total_sum += sum[i].value;
    }

    CHECK_EQUAL(val.value, nthreads * count * amount);
    CHECK_EQUAL_U64(total_sum, expected);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AtomicAddRet1_1) {
    return atomic_add_ret_test(1, 10000, 1);
}

DECLARE_TEST(AtomicAddRet1_10) {
    return atomic_add_ret_test(1, 10000, 10);
}

DECLARE_TEST(AtomicAddRet2_1) {
    return atomic_add_ret_test(2, 10000, 1);
}

DECLARE_TEST(AtomicAddRet10_1) {
    return atomic_add_ret_test(10, 10000, 1);
}

DECLARE_TEST(AtomicAddRet2_10) {
    return atomic_add_ret_test(2, 10000, 10);
}

DECLARE_TEST(AtomicAddRet10_10) {
    return atomic_add_ret_test(10, 5000, 10);
}

int
atomic_incr_test(int nthreads, int count)
{
    atomic_t barrier = 0;
    atomic_t val = 0;
    u_int32_t expected = 0;

    Thread* threads[nthreads];

    for (int i = 0; i < nthreads; ++i) {
        threads[i] = new IncrThread(&barrier, &val, count * (i + 1));
        expected += count * (i + 1);
        threads[i]->start();
    }

    while (barrier.value != (u_int)nthreads) {}
    barrier = 0;

    for (int i = 0; i < nthreads; ++i) {
        threads[i]->join();
        delete threads[i];
    }

    int errno_; const char* strerror_;

    CHECK_EQUAL(val.value, expected);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AtomicInc2) {
    return atomic_incr_test(2, 10000000);
}

DECLARE_TEST(AtomicInc10) {
    return atomic_incr_test(10, 10000000);
}

DECLARE_TEST(AtomicDecrTest) {
    atomic_t barrier = 0;
    atomic_t val = 0;
    atomic_t nzeros = 0;
    u_int32_t maxval = 0;

    int count = 10000000;
                
    IncrThread it(&barrier, &val, count, count / 1000);
    DecrTestThread dt(&barrier, &val, count, &nzeros, &maxval);

    it.start();
    dt.start();

    while (barrier.value != 2) {}
    barrier = 0;

    it.join();
    dt.join();

    CHECK_EQUAL(val.value, 0);
    CHECK_GTU(nzeros.value, 0);
    CHECK_GTU(maxval, 0);

    return UNIT_TEST_PASSED;
}

int
compare_and_swap_test(int nthreads, int count) {
    atomic_t barrier = 0;
    atomic_t val = 0;
    
    atomic_t success[nthreads];
    atomic_t fail[nthreads];
    Thread* threads[nthreads];

    for (int i = 0; i < nthreads; ++i) {
        success[i] = 0;
        fail[i]    = 0;
        threads[i] = new CompareAndSwapThread(&barrier, &val, count,
                                              &success[i], &fail[i]);
        threads[i]->start();
    }

    while (barrier.value != (u_int)nthreads) {}
    barrier = 0;

    for (int i = 0; i < nthreads; ++i) {
        threads[i]->join();
        delete threads[i];
    }

    int errno_; const char* strerror_;
    
    u_int32_t total_success = 0, total_fail = 0;
    for (int i = 0; i < nthreads; ++i) {
        CHECK_GTU(success[i].value, 0);
        CHECK_GTU(fail[i].value, 0);

        total_success += success[i].value;
        total_fail    += fail[i].value;
    }

    CHECK_EQUAL(total_success + total_fail, count * nthreads);
    CHECK_EQUAL(val.value, 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(CompareAndSwapTest2) {
    return compare_and_swap_test(2, 10000000);
}

DECLARE_TEST(CompareAndSwapTest10) {
    return compare_and_swap_test(10, 10000000);
}

DECLARE_TESTER(AtomicTester) {
    ADD_TEST(AllOps);
    ADD_TEST(AtomicAddRet1_1);
    ADD_TEST(AtomicAddRet1_10);
    ADD_TEST(AtomicAddRet2_1);
    ADD_TEST(AtomicAddRet10_1);
    ADD_TEST(AtomicAddRet2_10);
    ADD_TEST(AtomicAddRet10_10);
    ADD_TEST(AtomicInc2);
    ADD_TEST(AtomicInc10);
    ADD_TEST(AtomicDecrTest);
    ADD_TEST(CompareAndSwapTest2);
    ADD_TEST(CompareAndSwapTest10);
}

#else // __NO_ATOMIC__

DECLARE_TEST(Bogus) {
    log_warn("atomic test is meaningless without atomic.h");
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(AtomicTester) {
    ADD_TEST(Bogus);
}

#endif // __NO_ATOMIC__

DECLARE_TEST_FILE(AtomicTester, "atomic test");
