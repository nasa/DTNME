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
#include <errno.h>
#include <sys/time.h>

#include "debug/Log.h"
#include <thread/Mutex.h>
#include <thread/Thread.h>

using namespace oasys;


int invariant[1000];

class Thread1 : public Thread {
public:
    Thread1(Mutex* m) : Thread("Thread1"), mutex_(m) {}
    
protected:
    virtual void run() {
        struct timeval start, now;

        while(true)
        {
            ASSERT(! mutex_->is_locked_by_me());
            mutex_->lock("Thread1");
            ASSERT(mutex_->is_locked_by_me());

            fprintf(stderr, "Thread1: grabbed lock, sleeping\n");

            gettimeofday(&start, NULL);
            
            for(int i=0; i<1000; ++i)
            {
                invariant[i]++;
            }

            do {
                gettimeofday(&now, NULL);

                // reminds us of the Win3.1 days
                Thread::yield();
            } while(now.tv_sec - 1 < start.tv_sec);

            mutex_->unlock();

            // give thread 3 a chance
            if (invariant[0] % 4 == 0) {
                fprintf(stderr, "Thread1: sleeping to give Thread 3 a chance\n");
                sleep(4);
            }
        }
    }

    Mutex* mutex_;
};


class Thread2 : public Thread {
public:
    Thread2(Mutex* m) : Thread("Thread2"), mutex_(m) {}
    
protected:
    virtual void run() {
        struct timeval start, now;

        while(true)
        {
            ASSERT(!mutex_->is_locked_by_me());
            mutex_->lock("Thread2");
            ASSERT(mutex_->is_locked_by_me());

            fprintf(stderr, "Thread2: grabbed lock, sleeping\n");
            
            // recursive locking test
            {
                ScopeLock lock2(mutex_, "Thread2");
                gettimeofday(&start, NULL);
            }

            ASSERT(mutex_->is_locked_by_me());

            do {
                // assert invariant
                int val = invariant[0];
                for(int i=0; i<1000; i++)
                {
                    ASSERT(invariant[i] == val);
                }

                // reminds us of the Win3.1 days
                Thread::yield();

                gettimeofday(&now, NULL);
            } while(now.tv_sec - 0.3333 < start.tv_sec);

            mutex_->unlock();

            // give thread 3 a chance, but sleep for one more second
            // so thread 1 gets in there
            if (invariant[0] % 4 == 0) {
                fprintf(stderr, "Thread2: sleeping to give Thread 3 a chance\n");
                sleep(5);
            }
        }
    }

    Mutex* mutex_;
};

class Thread3 : public Thread {
public:
    Thread3(Mutex* m) : Thread("Thread3"), mutex_(m) {}
    
protected:
    virtual void run() {
        while(true) {
            int ret = mutex_->try_lock("Thread3");
            if (ret != EBUSY) {
                // very unlikely
                fprintf(stderr, "Thread3: grabbed lock, releasing and sleeping\n");
                ASSERT(mutex_->is_locked_by_me());
                mutex_->unlock();
                ASSERT(! mutex_->is_locked_by_me());
                sleep(2);
            } else {
                fprintf(stderr, "Thread3: missed lock, sleeping and trying again\n");
                ASSERT(! mutex_->is_locked_by_me());
                sleep(1);
            }

        }
    };

    Mutex* mutex_;
};


int
main()
{
    for(int i=0; i<1000; ++i)
    {
        invariant[i] = 0;
    }
    
    Log::init(LOG_DEBUG);

    Mutex mutex;
    
    Thread* t1 = new Thread1(&mutex);
    Thread* t2 = new Thread2(&mutex);
    Thread* t3 = new Thread3(&mutex);
    t1->start();
    t2->start();
    t3->start();
    
    while(true) {
        Thread::yield();
    }

}
