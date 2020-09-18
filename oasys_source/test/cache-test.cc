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

#include <pthread.h>
#include <stdlib.h>

#include "util/UnitTest.h"
#include "util/STLUtil.h"
#include "util/Cache.h"

using namespace oasys;

struct Helper {
    bool over_limit(const std::string& key, const int& value)
    {
        (void) key; (void) value;
        return elts_ + 1> max_;
    }

    void put(const std::string& key, const int& value)
    {
        (void) key; (void) value;
        ++elts_;
        birth_log_.push_back(value);
    }

    void cleanup(const std::string& key, const int& value)
    {
        (void) key; (void) value;
        --elts_;
        death_log_.push_back(value);
        log_notice_p("/test", "%d evicted", value);
    }

    size_t max_;
    size_t elts_;

    std::vector<int> birth_log_;
    std::vector<int> death_log_;
};

typedef Cache<std::string, int, Helper> TestCache;

DECLARE_TEST(Test1) {
    Helper h;
    
    h.max_  = 3;
    h.elts_ = 0;
    
    TestCache cache("/test-cache", h);
    
    cache.put_and_pin("a", 1);
    cache.unpin("a");
    cache.put_and_pin("b", 2);
    cache.unpin("b");
    cache.put_and_pin("c", 3);
    cache.put_and_pin("d", 4);
    cache.unpin("d");

    std::vector<int> c;
    CommaPushBack<std::vector<int>, int> pusher(&c);
    pusher = pusher, 1, 2, 3, 4;
    
    CHECK(cache.get_helper()->birth_log_ == c);
    
    c.clear();
    pusher = pusher, 1;
    CHECK(cache.get_helper()->death_log_ == c);

    cache.put_and_pin("e", 5);
    cache.unpin("e");
    cache.put_and_pin("f", 6);
    cache.unpin("f");
    cache.put_and_pin("g", 7);
    cache.unpin("g");

    c.clear();
    pusher = pusher, 1, 2, 3, 4, 5, 6, 7;
    CHECK(cache.get_helper()->birth_log_ == c);

    c.clear();
    pusher = pusher, 1, 2, 4, 5;
    CHECK(cache.get_helper()->death_log_ == c);

    int i;
    cache.get("f", &i);
    
    cache.put_and_pin("h", 8);
    cache.unpin("h");
    
    c.clear();
    pusher = pusher, 1, 2, 4, 5, 7;
    CHECK(cache.get_helper()->death_log_ == c);

    cache.put_and_pin("i", 9);
    cache.pin("i");
    cache.unpin("i");
    cache.unpin("i");

    c.clear();
    pusher = pusher, 1, 2, 4, 5, 7, 6;
    CHECK(cache.get_helper()->death_log_ == c);

    cache.evict("i");
    c.clear();
    pusher = pusher, 1, 2, 4, 5, 7, 6, 9;
    CHECK(cache.get_helper()->death_log_ == c);

    cache.unpin("c");
    cache.evict_all();
    c.clear();
    pusher = pusher, 1, 2, 4, 5, 7, 6, 9, 3, 8;
    CHECK(cache.get_helper()->death_log_ == c);

    return UNIT_TEST_PASSED;
}

struct Helper2 {
    Helper2() : max_(100000), elts_(0) {}

    bool over_limit(int key, const int& value)
    {
        (void) key; (void) value;
        return elts_ + 1> max_;
    }

    void put(int key, const int& value)
    {
        (void) key; (void) value;
        ++elts_;
    }

    void cleanup(int key, const int& value)
    {
        (void) key; (void) value;
        --elts_;
    }

    size_t max_;
    size_t elts_;
};
typedef Cache<int, int, Helper2> ThreadCache;

struct Params {
    int          max_elements_;
    int          iterations_;
    ThreadCache* cache_;
};

class WorkerThread : public Thread {
public:
    WorkerThread() : Thread("WorkerThread", CREATE_JOINABLE) {}
    void set_params(Params* params) { params_ = params; }
    
    void run()
    {
        for (int i = 0; i < params_->iterations_; ++i)
        {
            int cache_elem = random() % params_->max_elements_;
            // pin and unpin entries in the cache randomly
            int j = 1;
            int val;
            ThreadCache::Handle handle;

            params_->cache_->get_and_pin(cache_elem, &val, &handle);
            do 
            {
                if ( (random() % 10) > 6)
                {
                    handle.pin();
                    ++j;
                }
                else
                {
                    handle.unpin();
                    --j;
                }
            } while (j>0);

            if (i%10000 == 0)
            {
                log_notice_p("/test", "mark itr=%d, cache_elem=%d", i, cache_elem);
            }
        }
    }

    Params* params_;
};

DECLARE_TEST(Test2) {
    // Multithreading threading test
    Params p;
    ThreadCache cache("/test", Helper2());

    p.max_elements_= 10000;
    p.iterations_  = 500000;
    p.cache_       = &cache;

    for (int i = 0; i<p.max_elements_; ++i)
    {
        cache.put_and_pin(i, i);
        cache.unpin(i);
    }

    struct timeval begin;
    gettimeofday(&begin, NULL);

    WorkerThread thread[10];
    for (int i=0; i<10; ++i)
    {
        thread[i].set_params(&p);
        thread[i].start();
    }

    for (int i=0; i<10; ++i)
    {
        thread[i].join();
    }

    struct timeval end;
    gettimeofday(&end, NULL);

    log_notice_p("/test", "time = %d seconds", (int)(end.tv_sec - begin.tv_sec));

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Test1);
    ADD_TEST(Test2);
}

DECLARE_TEST_FILE(Test, "cache test");
