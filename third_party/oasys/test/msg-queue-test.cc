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

#include <limits.h>
#include "thread/Thread.h"
#include "thread/MsgQueue.h"
#include "util/UnitTest.h"

using namespace std;
using namespace oasys;

class Consumer : public Thread {
public:
    Consumer(MsgQueue<int>* q, int count)
        : Thread("Consumer", CREATE_JOINABLE),
          total_(0), q_(q), count_(count) {}

    int total_;

protected:
    virtual void run() {
        test();
    }

    int test() {
        int errno_; const char* strerror_; 
        int prev = -1;

        for (int i = 0; i < count_; ++i) {
            int curr = q_->pop_blocking();
            if (curr != prev + 1) {
                // check silently, then generate the error
                CHECK_EQUAL(curr, prev + 1);
            }
            prev = curr;
            total_++;
            yield();
        }

        return UNIT_TEST_PASSED;
    }
    
    MsgQueue<int>* q_;
    int count_;
};

class Producer : public Thread {
public:
    Producer(MsgQueue<int>* q, int count)
        : Thread("Producer", CREATE_JOINABLE),
          total_(0), q_(q), count_(count) {}

    int total_;
    
protected:
    virtual void run() {
        for (int i = 0; i < count_; ++i) {
            if (should_stop()) {
                return;
            }
            
            q_->push(total_++);
            yield();
        }
    }
    
    MsgQueue<int>* q_;
    int count_;
};

DECLARE_TEST(Init) {
    // Quiet down the spin lock
    SpinLock::warn_on_contention_ = false;

    log_always_p("/test", "flamebox-ignore ign pipe appears to be full");;
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Fini) {
    log_always_p("/test", "flamebox-ignore-cancel ign");;
    return UNIT_TEST_PASSED;
}

int
push_pop_test(int count) {
    MsgQueue<int> q("/test/queue");

    Consumer c(&q, count);
    Producer p(&q, count);

    c.start();
    p.start();

    p.join();
    c.join();

    int errno_; const char* strerror_;
    
    CHECK_EQUAL(p.total_, count);
    CHECK_EQUAL(c.total_, count);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(PushPop1) {
    return push_pop_test(1);
}

DECLARE_TEST(PushPop10000) {
    return push_pop_test(10000);
}

DECLARE_TEST(FullPipe) {
#ifdef __CYGWIN__
    log_warn_p("/test", "Cygwin doesn't implement full pipes properly...");
    return UNIT_TEST_PASSED;

#else
    MsgQueue<int> q("/test/queue");

    // fill up the pipe
    Producer p(&q, INT_MAX);
    p.start();

    int prev;
    while (1) {
        prev = p.total_;
        sleep(1);
        
        // nothing more pushed in the last second, so p is likely be blocked
        if (prev == p.total_) {
            break;
        }
    }

    int total = p.total_;

    p.set_should_stop();

    Consumer c(&q, total);
    c.start();

    p.join();
    c.join();

    CHECK_EQUAL(c.total_, total);
    CHECK_EQUAL(p.total_, total);

    return UNIT_TEST_PASSED;
#endif
}

DECLARE_TEST(NotifyWhenEmpty) {
    MsgQueue<int> q("/test/queue");

    q.notify_when_empty();
    Producer p(&q, 1000000);
    p.start();
    p.join();
    
    Consumer c(&q, 1000000);
    c.start();
    c.join();

    CHECK_EQUAL(c.total_, 1000000);
    CHECK_EQUAL(p.total_, 1000000);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(MsgQueueTester) {
    ADD_TEST(Init);
    ADD_TEST(PushPop1);
    ADD_TEST(PushPop10000);
    ADD_TEST(FullPipe);
    ADD_TEST(NotifyWhenEmpty);
    ADD_TEST(Fini);
}

DECLARE_TEST_FILE(MsgQueueTester, "msg queue test");
