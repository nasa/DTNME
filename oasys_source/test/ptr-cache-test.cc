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

#include "util/UnitTest.h"
#include "util/PointerHandle.h"
#include "util/LRUList.h"

using namespace oasys;

struct Name {};

class PtrCacheTest : public PointerHandle<int> {
public:
    PtrCacheTest(int* i)
        : orig_ptr_(i)
    {
        restore();
    }

    ~PtrCacheTest() { 
        invalidate();
    }
    
    int* orig_ptr() { return ptr_; }

protected:
    typedef LRUList<PtrCacheTest*> CacheList;
    static CacheList lru_;

    int* orig_ptr_;

    void invalidate() {
        printf("invalidate %p, %p\n", this, ptr_);
        
        if (ptr_ == 0)
            return;

        CacheList::iterator i = std::find(lru_.begin(), lru_.end(), this);
        ASSERT(i != lru_.end());

        lru_.erase(i);
        ptr_ = 0;
    }

    void restore() {
        printf("restore %p\n", this);

        ASSERT(std::find(lru_.begin(), lru_.end(), this) == lru_.end());

        ptr_ = orig_ptr_;
        lru_.push_back(this);

        while (lru_.size() > 3) {
            PtrCacheTest* evict = lru_.front();
            evict->invalidate();
        }
    }

    void update() {
        printf("update %p\n", this);

        CacheList::iterator i = std::find(lru_.begin(), lru_.end(), this);
        ASSERT(i != lru_.end());
        lru_.move_to_back(i);
    }
};

PtrCacheTest::CacheList PtrCacheTest::lru_;

DECLARE_TEST(Test1) {
    int a, b, c, d, e, f, g;
    PtrCacheTest pa(&a), pb(&b), pc(&c), pd(&d), pe(&e);
    PtrCacheTest pf(&f), pg(&g);
    
    CHECK(pa.orig_ptr() == 0);
    CHECK(pb.orig_ptr() == 0);
    CHECK(pc.orig_ptr() == 0);
    CHECK(pd.orig_ptr() == 0);
    CHECK(pe.orig_ptr() == &e);
    CHECK(pf.orig_ptr() == &f);
    CHECK(pg.orig_ptr() == &g);

    CHECK(pa.ptr() == &a);
    CHECK(pa.ptr() == &a);
    CHECK(pc.ptr() == &c);
    CHECK(pd.ptr() == &d);
    CHECK(pb.ptr() == &b);
    CHECK(pb.ptr() == &b);
    CHECK(pd.ptr() == &d);
    CHECK(pe.ptr() == &e);
    CHECK(pf.ptr() == &f);
    CHECK(pg.ptr() == &g);
    CHECK(pc.ptr() == &c);
    CHECK(pe.ptr() == &e);
    

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(PtrCacheTester) {
    ADD_TEST(Test1);
}

DECLARE_TEST_FILE(PtrCacheTester, "pointer cache test");
