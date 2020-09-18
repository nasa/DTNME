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

#include <unistd.h>
#include <debug/DebugUtils.h>
#include <thread/Thread.h>
#include <thread/LockDebugger.h>
#include <thread/SpinLock.h>
#include <thread/Mutex.h>

using namespace oasys;

Mutex* lock1;
Mutex* lock2;
Mutex* lock3;
SpinLock* lock4;
SpinLock* lock5;

class MyThread : public Thread {
public:
    MyThread() : Thread("MyThread", CREATE_JOINABLE) {}

protected:
    void run() {
        for (int i=0; i<100000; ++i) 
        {
            ASSERT(! Thread::lock_debugger()->check_class("m"));
            ASSERT(! Thread::lock_debugger()->check_class("s"));

            Thread::lock_debugger()->check();
            lock1->lock("test");
            lock2->lock("test");
            ASSERT(Thread::lock_debugger()->check_class("m"));
            ASSERT(! Thread::lock_debugger()->check_class("s"));

            LOCKED_BY_ME(lock1, lock2);
            lock5->lock("test");
            LOCKED_BY_ME(lock1, lock2, lock5);
            
            lock1->unlock();
            lock5->unlock();
            LOCKED_BY_ME(lock2);
            ASSERT(Thread::lock_debugger()->check_class("m"));
            ASSERT(! Thread::lock_debugger()->check_class("s"));
            lock2->unlock();
            ASSERT(! Thread::lock_debugger()->check_class("m"));
            ASSERT(! Thread::lock_debugger()->check_class("s"));
            LOCKED_BY_ME();
        }
    }
};

int
main()
{
    Log::init(LOG_DEBUG);
    
    MyThread t1, t2;
    
    lock1 = new Mutex("/test/lock1", Mutex::TYPE_RECURSIVE, true, "m");
    lock2 = new Mutex("/test/lock2", Mutex::TYPE_RECURSIVE, true, "m");
    lock3 = new Mutex("/test/lock3", Mutex::TYPE_RECURSIVE, true, "m");
    lock4 = new SpinLock("s");
    lock5 = new SpinLock("s");

    t1.start();
    t2.start();
    
    Thread::lock_debugger()->check();
    for (int i=0; i<100000; ++i) 
    {
        Thread::lock_debugger()->check();
        lock2->lock("test");
        lock3->lock("test");
        LOCKED_BY_ME(lock2, lock3);
        lock4->lock("test");
        LOCKED_BY_ME(lock2, lock3, lock4);
        lock5->lock("test");
        LOCKED_BY_ME(lock2, lock3, lock4, lock5);

        ASSERT(Thread::lock_debugger()->check_class("m"));
        ASSERT(Thread::lock_debugger()->check_class("s"));

        lock2->unlock();
        lock5->unlock();
        Thread::lock_debugger()->check(lock3, lock4);
        lock4->unlock();
        Thread::lock_debugger()->check(lock3);
        ASSERT(Thread::lock_debugger()->check_class("m"));
        ASSERT(! Thread::lock_debugger()->check_class("s"));

        lock3->unlock();
        Thread::lock_debugger()->check();
        ASSERT(! Thread::lock_debugger()->check_class("m"));
        ASSERT(! Thread::lock_debugger()->check_class("s"));
    }

    t1.join();
    t2.join();
}
