/*
 *    Copyright 2015 United States Government as represented by NASA
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
#  include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>
#include <oasys/util/Time.h>
#include <oasys/util/Random.h>

#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleListStrMultiMap.h"
#include "bundling/BundleTimestamp.h"

using namespace oasys;
using namespace dtn;

#define COUNT 10
#define MANY  10000

Bundle* bundles[MANY];
u_int32_t expected_refcount[MANY];
u_int32_t expected_mappings[MANY];

BundleListStrMultiMap::iterator iter;
BundleMappings::iterator map_iter;

BundleListStrMultiMap *l1, *l2, *l3;


DECLARE_TEST(Init) {

    BundleDaemon::init();

    BundleTimestamp bts;
    for (int i = 0; i < MANY; ++i) {
        bundles[i] = new Bundle(oasys::Builder::builder());
        bundles[i]->mutable_source()->assign("dtn://test/src");
        bundles[i]->test_set_bundleid(i);
        bts.seconds_ = bundles[i]->creation_ts().seconds_;
        bts.seqno_ = i;
        bundles[i]->set_creation_ts(bts);
        bundles[i]->mutable_payload()->init(i, BundlePayload::NODATA);
        expected_refcount[i] = 0;
        expected_mappings[i] = 0;
    }
    for (int i = 0; i < MANY; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
    }

    for (int i = 0; i < MANY; ++i) {
        bundles[i]->add_ref("test");
        // recount = 2 - 1 for "test" and 1 for auto add to all_bundles_ list
        expected_refcount[i] += 2;
        // mappings = 1 for auto add to all_bundles_ list
        expected_mappings[i] += 1;
    }
    for (int i = 0; i < MANY; ++i) {
        // all bundles should have been added to the all_bundles_ list
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
    }

    l1 = new BundleListStrMultiMap("list1");
    l2 = new BundleListStrMultiMap("list2");
    l3 = new BundleListStrMultiMap("list3");

    CHECK(l1->empty());
    CHECK(l2->empty());
    CHECK(l3->empty());
    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), 0);
    CHECK_EQUAL(l3->size(), 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BasicPushPop) {
    BundleRef bref("temp");
    Bundle* b;

    CHECK(l1->front() == NULL);
    CHECK(l1->back() == NULL);

    l1->insert(bundles[0]);
    expected_mappings[0] += 1;
    expected_refcount[0] += 1;
    CHECK(l1->front() == bundles[0]);
    CHECK(l1->back()  == bundles[0]);
    CHECK(l1->size() == 1);
    CHECK(bundles[0]->num_mappings() == expected_mappings[0]);

    bref = l1->pop_front();
    b = bref.object();
    bref = NULL;
    expected_mappings[0] -= 1;
    expected_refcount[0] -= 1;
    CHECK(l1->front() == NULL);
    CHECK(l1->back() == NULL);
    CHECK(l1->size() == 0);
    CHECK(bundles[0]->num_mappings() == expected_mappings[0]);
    CHECK(b == bundles[0]);
    b = NULL;

    for (int i = 0; i < COUNT; ++i) {
        l1->insert(bundles[i]);
        expected_mappings[i] += 1;
        expected_refcount[i] += 1;
        CHECK(l1->back() == bundles[i]);
        CHECK(bundles[i]->is_queued_on(l1));
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    CHECK(l1->front() == bundles[0]);
    CHECK(l1->back() == bundles[COUNT-1]);

    // NOTE: this only works for the first 10 bundles after which they will not be in order
    for (int i = 0; i < COUNT; ++i) {
        bref = l1->pop_front();
        b = bref.object();
        bref = NULL;
        expected_mappings[i] -= 1;
        expected_refcount[i] -= 1;
        CHECK(b == bundles[i]);
        CHECK_EQUAL(b->refcount(), expected_refcount[i]);
        b = NULL;
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    CHECK(l1->front() == NULL);
    CHECK(l1->back() == NULL);
    
    CHECK(l1->empty());
    CHECK_EQUAL(l1->size(), 0);

    return UNIT_TEST_PASSED;
}


DECLARE_TEST(ContainsAndErase) {
    for (int i = 0; i < COUNT; ++i) {
        l1->insert(bundles[i]);
        expected_mappings[i] += 1;
        expected_refcount[i] += 1;
        CHECK(l1->contains(bundles[i]));
    }
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT);

    CHECK(l1->erase(bundles[0]));
    expected_mappings[0] -= 1;
    expected_refcount[0] -= 1;
    CHECK(! l1->contains(bundles[0]));
    CHECK_EQUAL(bundles[0]->refcount(), expected_refcount[0]);
    CHECK_EQUAL(bundles[0]->num_mappings(), expected_mappings[0]);
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT - 1);
    
    CHECK(! l1->erase(bundles[0]));
    CHECK(! l1->contains(bundles[0]));
    CHECK_EQUAL(bundles[0]->refcount(), expected_refcount[0]);
    CHECK_EQUAL(bundles[0]->num_mappings(), expected_mappings[0]);
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT - 1);

    CHECK(l1->erase(bundles[5]));
    expected_mappings[5] -= 1;
    expected_refcount[5] -= 1;
    CHECK(! l1->contains(bundles[5]));
    CHECK_EQUAL(bundles[5]->refcount(), expected_refcount[5]);
    CHECK_EQUAL(bundles[5]->num_mappings(), expected_mappings[5]);
    CHECK(! bundles[5]->is_queued_on(l1));
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT - 2);

    // test using an iterator
    l1->lock()->lock("test lock");
    iter = l1->begin();
    iter++; iter++; iter++;
    CHECK(iter->second == bundles[4]);
    DO(l1->erase(iter));
    expected_mappings[4] -= 1;
    expected_refcount[4] -= 1;
    CHECK(! l1->contains(bundles[4]));
    CHECK_EQUAL(bundles[4]->refcount(), expected_refcount[4]);
    CHECK_EQUAL(bundles[4]->num_mappings(), expected_mappings[4]);
    CHECK(! bundles[4]->is_queued_on(l1));
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT - 3);
    l1->lock()->unlock();

    CHECK(! l1->contains(NULL));
    CHECK(! l1->erase(NULL));
    CHECK(!l1->empty());
    CHECK_EQUAL(l1->size(), COUNT - 3);

    l1->clear();
    for (int i = 0; i < COUNT; ++i)
    {
        // update expecteds for bundles that were not
        // previously removed from the queue
        if ( i != 0 && i != 4 && i != 5 )
        {
            expected_mappings[i] -= 1;
            expected_refcount[i] -= 1;
        }
    }
    CHECK(l1->empty());
    CHECK_EQUAL(l1->size(), 0);
    for (int i = 0; i < COUNT; ++i) {
        CHECK(! l1->contains(bundles[i]));
        CHECK(! l1->erase(bundles[i]));
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultipleLists) {
    Bundle* b;
    BundleListStrMultiMap* l;
    BundleListBase* lb;
    
    for (int i = 0; i < COUNT; ++i) {
        l1->insert(bundles[i]);
        expected_mappings[i] += 1;
        expected_refcount[i] += 1;

        if ((i % 2) == 0) {
            l2->insert(bundles[i]);
            expected_mappings[i] += 1;
            expected_refcount[i] += 1;
        } else {
            l2->insert(bundles[i]);
            expected_mappings[i] += 1;
            expected_refcount[i] += 1;
        }

        if ((i % 3) == 0) {
            l3->insert(bundles[i]);
            expected_mappings[i] += 1;
            expected_refcount[i] += 1;
        } else if ((i % 3) == 1) {
            l3->insert(bundles[i]);
            expected_mappings[i] += 1;
            expected_refcount[i] += 1;
        }
    }

    b = bundles[0];
    CHECK_EQUAL(b->num_mappings(), expected_mappings[0]);
    CHECK_EQUAL(b->refcount(), expected_refcount[0]);
    b->lock()->lock("test lock");

    for (map_iter = b->mappings()->begin();
         map_iter != b->mappings()->end();
         ++map_iter)
    {
        SPBMapping bmap ( *map_iter );
        lb = (BundleListBase*) bmap->list();
        if ( ( l = dynamic_cast<BundleListStrMultiMap *>(lb) ) )
        {
            SPV vpos = bmap->position();
            BundleListStrMultiMap::iterator* bitr = (BundleListStrMultiMap::iterator*)vpos.get();
            Bundle* b2 = (*bitr)->second;
            CHECK(b2 == bundles[0]);
            CHECK(b2 == bundles[0]);
            CHECK(l->contains(b2));
        }
    }

    bool done = false;
    while ( ! done )
    {
        done = true;
        map_iter = b->mappings()->begin();
        while ( map_iter != b->mappings()->end() ) 
        {
            SPBMapping bmap ( *map_iter );
            ++map_iter;

            lb = (BundleListBase*) bmap->list();

             // skip the all_bundles_list entry
            if ( ( l = dynamic_cast<BundleListStrMultiMap *>(lb) ) )
            {
                b->lock()->unlock();
                CHECK(l->erase(b));
                expected_mappings[0] -= 1;
                expected_refcount[0] -= 1;

                b->lock()->lock("test lock");
                CHECK(! l->contains(b));
                done = false;
                break;
            }
        }
    }

    b->lock()->unlock();
    CHECK_EQUAL(b->num_mappings(), expected_mappings[0]);
    CHECK_EQUAL(b->refcount(), expected_refcount[0]);

    // list contents fall through to next test
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultipleListRemoval) {
    Bundle* b;
    BundleListBase* lb;
    BundleListStrMultiMap* l;

    int refidx;
    bool found;

    l3->lock()->lock("test lock");
    for (iter = l3->begin(); iter != l3->end(); )
    {
        b = iter->second;
        ++iter; // increment before removal

        b->lock()->lock("test lock");

        // find the index of this bundleref
        found = false;
        for ( refidx=0; refidx<COUNT; ++refidx ) 
        {
          if ( bundles[refidx] == b )
          {
            found = true;
            break;
          }
        }
        ASSERT(found);
        CHECK_EQUAL(b->num_mappings(), expected_mappings[refidx]);
        CHECK_EQUAL(b->refcount(), expected_refcount[refidx]);

        bool done = false;
        while ( ! done )
        {
            done = true;

            map_iter = b->mappings()->begin();
            while ( map_iter != b->mappings()->end() ) 
            {
                SPBMapping bmap ( *map_iter );
                ++map_iter;

                lb = (BundleListBase*) bmap->list();

                 // skip the all_bundles_list entry
                if ( typeid(BundleListStrMultiMap) == typeid(*lb) )
                {
                    l = (BundleListStrMultiMap*) lb;
    
                    b->lock()->unlock();
                    CHECK(l->erase(b));
                    expected_mappings[refidx] -= 1;
                    expected_refcount[refidx] -= 1;
    
                    b->lock()->lock("test lock");
                    CHECK(! l->contains(b));
                    done = false;
                    break;
                }
            }
        }

        b->lock()->unlock();
    }

    l3->lock()->unlock();

    ASSERT(l3->size() == 0);

    for (int i = 0; i < COUNT; ++i) {
        if (i == 0)
            continue;
        
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    l1->clear();
    l2->clear();
    /* adjust the expected values as a result of the clears */
    for (int i=0; i<COUNT; ++i) {
        if ( 2 == i % 3 ) {
            expected_mappings[i] -= 2;
            expected_refcount[i] -= 2;
        }
    }


    ASSERT(l1->size() == 0);
    ASSERT(l2->size() == 0);

    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MoveContents) {
    // now start the test
    for (int i = 0; i < COUNT; ++i) {
        l1->insert(bundles[i]);
        expected_mappings[i] += 1;
        expected_refcount[i] += 1;
    }
    CHECK_EQUAL(l1->size(), COUNT);
    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }
    
    l1->move_contents(l2);
    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), COUNT);
    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    l2->clear();
    for (int i = 0; i < COUNT; ++i) {
        expected_mappings[i] -= 1;
        expected_refcount[i] -= 1;
    }
    CHECK_EQUAL(l1->size(), 0);
    CHECK_EQUAL(l2->size(), 0);
    for (int i = 0; i < COUNT; ++i) {
        CHECK_EQUAL(bundles[i]->num_mappings(), expected_mappings[i]);
        CHECK_EQUAL(bundles[i]->refcount(), expected_refcount[i]);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ManyBundles) {
    for (int i = 0; i < MANY; ++i) {
        l1->insert(bundles[i]);
    }
    
    oasys::PermutationArray a(MANY);
    oasys::Time t;
    
    log_always_p("/test", "testing %u erasures with mapping code", MANY);
    bool ok = true;
    t.get_time();
    for (int i = 0; i < MANY; ++i) {
        ok = ok && l1->erase(bundles[a.map(i)]);
    }
    log_always_p("/test", "elapsed time: %u ms", t.elapsed_ms());
    CHECK(ok);
    CHECK(l1->size() == 0);

    for (int i = 0; i < MANY; ++i) {
        l1->insert(bundles[i]);
    }
    
    log_always_p("/test", "testing %u erasures with linear code", MANY);
    t.get_time();
    l1->lock()->lock("test lock");
    for (int i = 0; i < MANY; ++i) {
        Bundle* b = bundles[a.map(i)];
        for (iter = l1->begin(); iter != l1->end(); ++iter) {
            if (iter->second == b) {
                l1->erase(iter);
                break;
            }
        }
    }
    l1->lock()->unlock();
    log_always_p("/test", "elapsed time: %u ms", t.elapsed_ms());
    CHECK(l1->size() == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(BundleListTest) {
    ADD_TEST(Init);
    ADD_TEST(BasicPushPop);
    ADD_TEST(ContainsAndErase);
    ADD_TEST(MultipleLists);
    ADD_TEST(MultipleListRemoval);
    ADD_TEST(MoveContents);
    ADD_TEST(ManyBundles);
}

DECLARE_TEST_FILE(BundleListTest, "bundle list test");
