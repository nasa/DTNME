/*
 *    Copyright 2006 Intel Corporation
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

//
// NOTE: This file is not intended to be executed on its own... rather
// it is intended to be included by a datastore-specific test, such as
// berkeley-db-test or filesys-db-test, each of which define the
// macros NEW_DS_IMPL() and sets up a handful of global parameters
//

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <bitset>

#include "util/UnitTest.h"
#include "util/StringBuffer.h"
#include "util/Random.h"
#include "storage/StorageConfig.h"
#include "storage/DurableStore.h"
#include "serialize/TypeShims.h"

using namespace oasys;

DurableStore*  g_store  = 0;
StorageConfig* g_config = 0;

typedef SingleTypeDurableTable<StringShim> StringDurableTable;
typedef DurableObjectCache<StringShim> StringDurableCache;

struct TestC {};

TYPE_COLLECTION_INSTANTIATE(TestC);

class Obj : public oasys::SerializableObject {
public:
    Obj(int32_t id, const char* static_name)
        : id_(id), static_name_(static_name) {}
    Obj(const Builder&) : id_(0), static_name_("no name") {}
    
    virtual const char* name() = 0;

    virtual void serialize(SerializeAction* a) {
        a->process("name", &static_name_);
        a->process("id", &id_);
    }

    int32_t id_;
    std::string static_name_;
};

class Foo : public Obj {
public:
    static const int ID = 1;
    Foo(const char* name = "foo") : Obj(ID, name) {}
    Foo(const Builder& b) : Obj(b) {}
    virtual const char* name() { return "foo"; }
};

class Bar : public Obj {
public:
    static const int ID = 2;
    Bar(const char* name = "bar") : Obj(ID, name) {}
    Bar(const Builder& b) : Obj(b) {}
    virtual const char* name() { return "bar"; }
};

TYPE_COLLECTION_DECLARE(TestC, Foo, 1);
TYPE_COLLECTION_DECLARE(TestC, Bar, 2);
TYPE_COLLECTION_GROUP(TestC, Obj, 1, 2);

TYPE_COLLECTION_DEFINE(TestC, Foo, 1);
TYPE_COLLECTION_DEFINE(TestC, Bar, 2);

#define TYPECODE_FOO (TypeCollectionCode<TestC, Foo>::TYPECODE)
#define TYPECODE_BAR (TypeCollectionCode<TestC, Bar>::TYPECODE)

typedef MultiTypeDurableTable<Obj, TestC> ObjDurableTable;
typedef DurableObjectCache<Obj> ObjDurableCache;

DECLARE_TEST(DBInit) {
    DurableStore* store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    DEL_DS_STORE(store);

    return 0;
}

DECLARE_TEST(DBTidy) {
    // do tidy
    g_config->tidy_ = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table1 = 0;    
    CHECK(store->get_table(&table1, "test", DS_CREATE | DS_EXCL) == 0);
    CHECK(table1 != 0);

    delete_z(table1);
    DEL_DS_STORE(store);

    // don't do tidy
    g_config->tidy_ = false;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    CHECK(store->get_table(&table1, "test", 0) == 0);
    CHECK(table1 != 0);
    delete_z(table1);
    DEL_DS_STORE(store);

    // do tidy
    g_config->tidy_ = true;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    CHECK(store->get_table(&table1, "test", 0) == DS_NOTFOUND);
    CHECK(table1 == 0);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TableCreate) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
    
    StringDurableTable* table1 = 0;
    StringDurableTable* table2 = 0;
    ObjDurableTable*    objtable = 0;

    CHECK(store->get_table(&table1, "test", 0) == DS_NOTFOUND);
    CHECK(table1 == 0);
    
    CHECK(store->get_table(&table1, "test", DS_CREATE) == 0);
    CHECK(table1 != 0);
    delete_z(table1);
    
    CHECK(store->get_table(&table2, "test", DS_CREATE | DS_EXCL) 
          == DS_EXISTS);
    CHECK(table2 == 0);
    
    CHECK(store->get_table(&table2, "test", 0) == 0);
    CHECK(table2 != 0);
    delete_z(table2);

    CHECK(store->get_table(&table1, "test", DS_CREATE) == 0);
    CHECK(table1 != 0);
    delete_z(table1);

    CHECK(store->get_table(&objtable, "objtable", DS_CREATE | DS_EXCL) == 0);
    CHECK(objtable != 0);
    delete_z(objtable);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}


DECLARE_TEST(TableGetNames) {
    g_config->tidy_ = true;

    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
    
    StringDurableTable* table1 = 0;
    StringDurableTable* table2 = 0;
    StringDurableTable* table3 = 0;
    StringDurableTable* table4 = 0;

    CHECK(store->get_table(&table1, "test1", DS_CREATE) == 0);
    CHECK(store->get_table(&table2, "test2", DS_CREATE) == 0);
    CHECK(store->get_table(&table3, "test3", DS_CREATE) == 0);
    CHECK(store->get_table(&table4, "test4", DS_CREATE) == 0);

    StringVector names;
    CHECK(store->get_table_names(&names) == 0);
    
    CHECK(std::find(names.begin(), names.end(), "test1") != names.end());
    CHECK(std::find(names.begin(), names.end(), "test2") != names.end());
    CHECK(std::find(names.begin(), names.end(), "test3") != names.end());
    CHECK(std::find(names.begin(), names.end(), "test4") != names.end());

    delete_z(table1);
    delete_z(table2);
    delete_z(table3);
    delete_z(table4);

    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TableDelete) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
    
    StringDurableTable* table = 0;

    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    CHECK(table != 0);
    delete_z(table);

    CHECK(store->del_table("test") == 0);
    
    CHECK(store->get_table(&table, "test", 0) == DS_NOTFOUND);
    CHECK(table == 0);

    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    CHECK(table != 0);
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypePut) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);

    IntShim    key(99);
    StringShim data("data");

    CHECK(table->size() == 0);
    CHECK(table->put(key, &data, 0) == DS_NOTFOUND);
    CHECK(table->put(key, &data, DS_CREATE) == 0);
    CHECK(table->size() == 1);
    CHECK(table->put(key, &data, 0) == 0);
    CHECK(table->put(key, &data, DS_CREATE) == 0);
    CHECK(table->put(key, &data, DS_CREATE | DS_EXCL) == DS_EXISTS);
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypeGet) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);

    IntShim    key(99);
    IntShim    key2(101);
    StringShim data("data");
    StringShim* data2 = 0;

    CHECK(table->size() == 0);
    CHECK(table->put(key, &data, DS_CREATE | DS_EXCL) == 0);
    CHECK(table->size() == 1);
    
    CHECK(table->get(key, &data2) == 0);
    CHECK(data2 != 0);
    CHECK(data2->value() == data.value());

    delete_z(data2);
    data2 = 0;
    
    CHECK(table->get(key2, &data2) == DS_NOTFOUND);
    CHECK(data2 == 0);

    data.assign("new data");
    CHECK(table->put(key, &data, 0) == 0);

    CHECK(table->get(key, &data2) == 0);
    CHECK(data2 != 0);
    CHECK(data2->value() == data.value());

    delete_z(data2);
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypeDelete) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);

    IntShim    key(99);
    IntShim    key2(101);
    StringShim data("data");
    StringShim* data2;

    CHECK(table->put(key, &data, DS_CREATE | DS_EXCL) == 0);
    CHECK(table->size() == 1);
    
    CHECK(table->get(key, &data2) == 0);
    CHECK(data2 != 0);
    CHECK(data2->value() == data.value());
    delete_z(data2);
    data2 = 0;
    
    CHECK(table->del(key) == 0);
    CHECK(table->size() == 0);

    CHECK(table->get(key, &data2) == DS_NOTFOUND);
    CHECK(data2 == 0);

    CHECK(table->del(key2) == DS_NOTFOUND);
    CHECK(table->size() == 0);
    
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypeMultiObject) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);

    int num_objs = 100;
    
    for(int i=0; i<num_objs; ++i) {
        StaticStringBuffer<256> buf;
        buf.appendf("data%d", i);
        StringShim data(buf.c_str());
        
        CHECK(table->put(IntShim(i), &data, DS_CREATE | DS_EXCL) == 0);
    }
    CHECK((int)table->size() == num_objs);
    
    delete_z(table);

#ifndef __MEMORY_STORE_H__
    // special case for memory store
    DEL_DS_STORE(store);
    g_config->tidy_ = false;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
#endif

    CHECK(store->get_table(&table, "test", 0) == 0);

    PermutationArray pa(num_objs);

    for(int i=0; i<num_objs; ++i) {
        StaticStringBuffer<256> buf;
        StringShim* data = 0;

        IntShim key(pa.map(i));
        buf.appendf("data%d", pa.map(i));

        CHECK(table->get(key, &data) == 0);
        CHECK_EQUALSTR(buf.c_str(), data->value().c_str());
        delete_z(data);
    }
    CHECK((int)table->size() == num_objs);
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypeIterator) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    static const int num_objs = 100;
     
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    
    for(int i=0; i<num_objs; ++i) {
        StaticStringBuffer<256> buf;
        buf.appendf("data%d", i);
        StringShim data(buf.c_str());
        
        CHECK(table->put(IntShim(i), &data, DS_CREATE | DS_EXCL) == 0);
    }

    DurableIterator* iter = table->itr();

    std::bitset<num_objs> found;
    while(iter->next() == 0) {
        Builder b; // XXX/demmer can't use temporary ??
        IntShim key(b);
        
        iter->get_key(&key);
        CHECK(found[key.value()] == false);
        found.set(key.value());
    }

    found.flip();
    CHECK(!found.any());

    delete_z(iter); 
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;    
}

DECLARE_TEST(KeithMulti) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table1;
    StringDurableTable* table2;
    static const int num_objs = 10;
     
    CHECK(store->get_table(&table1, "test1", DS_CREATE | DS_EXCL) == 0);
    CHECK(store->get_table(&table2, "test2", DS_CREATE | DS_EXCL) == 0);
    
    for(int i=0; i<num_objs; ++i) {
        StaticStringBuffer<256> buf;
        buf.appendf("data%d", i);
        StringShim data(buf.c_str());
        
        CHECK(table1->put(IntShim(i), &data, DS_CREATE | DS_EXCL) == 0);
        CHECK(table2->put(IntShim(i), &data, DS_CREATE | DS_EXCL) == 0);
    }

    DurableIterator* iter = table1->itr();

    std::bitset<num_objs> found1;
    while(iter->next() == 0) {
        StaticStringBuffer<256> buf;

        Builder b; // XXX/demmer can't use temporary ??
        StringShim *val;
        IntShim key(b);
        bool thing;
	int ret;
        
        iter->get_key(&key);
        CHECK(found1[key.value()] == false);
        found1.set(key.value());

        //
        buf.appendf("data%d", key.value());
        ret = table1->get(IntShim(key.value()), &val, &thing);
        CHECK(strcmp(val->value().c_str(), buf.c_str())==0);
    }

    found1.flip();
    CHECK(!found1.any());
    delete_z(iter); 

    //
    //
    iter = table2->itr();

    std::bitset<num_objs> found2;
    while(iter->next() == 0) {
        StaticStringBuffer<256> buf;

        Builder b; // XXX/demmer can't use temporary ??
        StringShim *val;
        IntShim key(b);
        bool thing;
	int ret;
        
        iter->get_key(&key);
        CHECK(found2[key.value()] == false);
        found2.set(key.value());

        //
        buf.appendf("data%d", key.value());
        ret = table2->get(IntShim(key.value()), &val, &thing);
        CHECK(strcmp(val->value().c_str(), buf.c_str())==0);
    }

    found2.flip();
    CHECK(!found2.any());

    delete_z(iter); 
    delete_z(table1);
    delete_z(table2);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;    
}

DECLARE_TEST(SingleTypeIteratorAndGet) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StringDurableTable* table;
    static const int num_objs = 100;
     
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    
    for(int i=0; i<num_objs; ++i) {
        StaticStringBuffer<256> buf;
        buf.appendf("data%d", i);
        StringShim data(buf.c_str());
        
        CHECK(table->put(IntShim(i), &data, DS_CREATE | DS_EXCL) == 0);
    }

    DurableIterator* iter = table->itr();

    std::bitset<num_objs> found;
    while(iter->next() == 0) {
        StaticStringBuffer<256> buf;

        Builder b; // XXX/demmer can't use temporary ??
        StringShim *val;
        IntShim key(b);
        bool thing;
	int ret;
        
        iter->get_key(&key);
        CHECK(found[key.value()] == false);
        found.set(key.value());

        //
        printf("Read key value %d\n", key.value());
        buf.appendf("data%d", key.value());
        ret = table->get(IntShim(key.value()), &val, &thing);
        printf("ret(%d) val(%s)\n", ret, val->value().c_str());
        CHECK(strcmp(val->value().c_str(), buf.c_str())==0);
    }

    found.flip();
    CHECK(!found.any());

    delete_z(iter); 
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;    
}

DECLARE_TEST(MultiType) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    ObjDurableTable* table = 0;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    CHECK(table != 0);

    Obj *o1, *o2 = NULL;
    Foo foo;
    Bar bar;

    CHECK(table->put(StringShim("foo"), Foo::ID, &foo, 
                     DS_CREATE | DS_EXCL) == 0);
    CHECK(table->put(StringShim("bar"), Bar::ID, &bar, 
                     DS_CREATE | DS_EXCL) == 0);

    CHECK(table->get(StringShim("foo"), &o1) == 0);
    CHECK_EQUAL(o1->id_, Foo::ID);
    CHECK_EQUALSTR(o1->name(), "foo");
    CHECK_EQUALSTR(o1->static_name_.c_str(), "foo");
    CHECK(dynamic_cast<Foo*>(o1) != NULL);
    delete_z(o1);
    
    CHECK(table->get(StringShim("bar"), &o2) == 0);
    CHECK_EQUAL(o2->id_, Bar::ID);
    CHECK_EQUALSTR(o2->name(), "bar");
    CHECK_EQUALSTR(o2->static_name_.c_str(), "bar");
    CHECK(dynamic_cast<Bar*>(o2) != NULL);
    delete_z(o2);

    // Check mixed-up typecode and object
    CHECK(table->put(StringShim("foobar"), Bar::ID, &foo, 
                     DS_CREATE | DS_EXCL) == 0);
    CHECK(table->get(StringShim("foobar"), &o1) == 0);
    CHECK_EQUAL(o1->id_, Foo::ID);
    CHECK_EQUALSTR(o1->name(), "bar");
    CHECK_EQUALSTR(o1->static_name_.c_str(), "foo");
    CHECK(dynamic_cast<Foo*>(o1) == NULL);
    CHECK(dynamic_cast<Bar*>(o1) != NULL);
    delete_z(o1);
    
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(NonTypedTable) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);

    StaticTypedDurableTable* table = 0;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL) == 0);
    CHECK(table != 0);

    StringShim serial_string("01234567890");
    IntShim    serial_int(42);
    
    CHECK(table->put(StringShim("foo"), &serial_string, DS_CREATE) == 0);
    CHECK(table->put(StringShim("bar"), &serial_int, DS_CREATE) == 0);

    StringShim* str = 0;
    IntShim*    i   = 0;
    
    CHECK(table->get(StringShim("foo"), &str) == 0);
    CHECK(table->get(StringShim("bar"), &i)   == 0);

    CHECK_EQUALSTR(str->value().c_str(), "01234567890");
    CHECK_EQUAL(i->value(), 42);

    delete_z(str);
    delete_z(i);
    delete_z(table);
    DEL_DS_STORE(store);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SingleTypeCache) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
    StringDurableCache* cache = 
        new StringDurableCache("/test_storage/cache/string", 32);

    StringDurableTable* table;

    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL, cache) == 0);

    StringShim* s;
    StringShim* s1 = new StringShim("test");
    StringShim* s2 = new StringShim("test test");
    StringShim* s3 = new StringShim("test test test");
    StringShim* s4 = new StringShim("test test test test");

    CHECK(table->put(IntShim(1), s1, 0) == DS_NOTFOUND);
    CHECK_EQUAL(cache->size(), 0);
    
    CHECK(table->put(IntShim(1), s1, DS_CREATE) == DS_OK);
    CHECK_EQUAL(cache->size(), 8);

    CHECK(table->put(IntShim(1), s1, 0) == DS_OK);
    CHECK_EQUAL(cache->size(), 8);
    
    CHECK(table->put(IntShim(1), s1, DS_CREATE | DS_EXCL) == DS_EXISTS);
    CHECK_EQUAL(cache->size(), 8);
    CHECK_EQUAL(cache->count(), 1);
    
    // both these items put the cache over capacity
    log_notice_p("/test", "flamebox-ignore ign1 cache already at capacity");
    
    CHECK(table->put(IntShim(2), s2, DS_CREATE | DS_EXCL) == DS_OK);
    CHECK_EQUAL(cache->size(), 21);

    CHECK(table->put(IntShim(3), s3, DS_CREATE | DS_EXCL) == DS_OK);
    CHECK_EQUAL(cache->size(), 39);

    CHECK(table->put(IntShim(4), s4, DS_CREATE | DS_EXCL) == DS_OK);
    // cancel the warning one line later than would otherwise because
    // the cache warning doesn't pop up until one command later
    log_notice_p("/test", "flamebox-ignore-cancel ign1");
    CHECK_EQUAL(cache->size(), 62);

    CHECK_EQUAL(cache->count(), 4);
    CHECK_EQUAL(cache->live(), 4);

    // make sure all four objects are in cache
    CHECK(table->get(IntShim(1), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  1);
    CHECK(s == s1);

    CHECK(table->get(IntShim(2), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  2);
    CHECK(s == s2);
    
    CHECK(table->get(IntShim(3), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  3);
    CHECK(s == s3);
    
    CHECK(table->get(IntShim(4), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  4);
    CHECK(s == s4);

    // now release them all in reverse order which will cause s3 and
    // s4 to be evicted
    CHECK(cache->release(IntShim(4), s4) == DS_OK);
    CHECK(cache->release(IntShim(3), s3) == DS_OK);
    CHECK(cache->release(IntShim(2), s2) == DS_OK);
    CHECK(cache->release(IntShim(1), s1) == DS_OK);

    CHECK_EQUAL(cache->size(), 21);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 0);

    // make sure s1 and s2 are still in-cache
    CHECK(table->get(IntShim(1), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  5);
    CHECK(s == s1);

    CHECK(table->get(IntShim(2), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK(s == s2);

    // release s1 and s2, then try to get s3 and s4 which should
    // create new objects, leaving only s4 in cache when we're done
    CHECK(cache->release(IntShim(1), s1) == DS_OK);
    CHECK(cache->release(IntShim(2), s2) == DS_OK);

    CHECK(table->get(IntShim(3), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK_EQUAL(cache->misses(), 1);
    CHECK_EQUALSTR(s->value().c_str(), "test test test");
    CHECK_EQUAL(cache->evictions(), 3);
    CHECK(cache->release(IntShim(3), s) == DS_OK);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 0);
    
    CHECK(table->get(IntShim(4), &s) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK_EQUAL(cache->misses(), 2);
    CHECK_EQUALSTR(s->value().c_str(), "test test test test");
    CHECK_EQUAL(cache->evictions(), 5);
    CHECK(cache->release(IntShim(4), s) == DS_OK);
    CHECK_EQUAL(cache->count(), 1);
    CHECK_EQUAL(cache->live(), 0);

    // delete an object out of cache, a live object in cache (can't
    // do), and a dead object in cache
    CHECK(table->get(IntShim(1), &s) == DS_OK);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 1);

    log_notice_p("/test", "flamebox-ignore ign1 .*can't remove "
               "live object .* from cache");
    CHECK(table->del(IntShim(1)) == DS_ERR);
    log_notice_p("/test", "flamebox-ignore-cancel ign1");
    CHECK(table->get(IntShim(1), &s) == DS_OK);
    CHECK(cache->release(IntShim(1), s) == DS_OK);
    CHECK(table->del(IntShim(1)) == DS_OK);
    CHECK(table->get(IntShim(1), &s) == DS_NOTFOUND);
    CHECK(table->del(IntShim(2)) == DS_OK);
    CHECK(table->get(IntShim(2), &s) == DS_NOTFOUND);
    CHECK(table->del(IntShim(3)) == DS_OK);
    CHECK(table->get(IntShim(3), &s) == DS_NOTFOUND);
    CHECK(table->del(IntShim(4)) == DS_OK);
    CHECK(table->get(IntShim(4), &s) == DS_NOTFOUND);
    
    CHECK_EQUAL(cache->count(), 0);
    CHECK_EQUAL(cache->live(),  0);
    CHECK_EQUAL(cache->size(),  0);
    
    delete_z(table);
    DEL_DS_STORE(store);
    delete_z(cache);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultiTypeCache) {
    g_config->tidy_         = true;
    DurableStore* store;

    store = new DurableStore("/test_storage");
    CHECK(store->create_store(*g_config) == 0);
    ObjDurableCache*    cache = new ObjDurableCache("/test_storage/cache/obj", 36);

    ObjDurableTable* table = 0;
    CHECK(store->get_table(&table, "test", DS_CREATE | DS_EXCL, cache) == 0);

    Obj* o;
    Foo* f1 = new Foo("test");
    Foo* f2 = new Foo("test2");
    Bar* b1 = new Bar("test test");
    Bar* b2 = new Bar("test test test");

    CHECK(table->put(IntShim(1), Foo::ID, f1, 0) == DS_NOTFOUND);
    CHECK_EQUAL(cache->size(), 0);
    
    CHECK(table->put(IntShim(1), Foo::ID, f1, DS_CREATE) == DS_OK);
    CHECK_EQUAL(cache->size(), 12);

    CHECK(table->put(IntShim(1), Foo::ID, f1, 0) == DS_OK);
    CHECK_EQUAL(cache->size(), 12);
    
    CHECK(table->put(IntShim(1), Foo::ID, f1, 
                     DS_CREATE | DS_EXCL) == DS_EXISTS);
    CHECK_EQUAL(cache->size(), 12);
    CHECK_EQUAL(cache->count(), 1);
    
    // both these items put the cache over capacity
    log_notice_p("/test", "flamebox-ignore ign1 cache already at capacity");
    
    CHECK(table->put(IntShim(2), Foo::ID, f2, DS_CREATE | DS_EXCL) == DS_OK);
    CHECK_EQUAL(cache->size(), 25);

    CHECK(table->put(IntShim(3), Bar::ID, b1, DS_CREATE | DS_EXCL) == DS_OK);
    CHECK_EQUAL(cache->size(), 42);

    CHECK(table->put(IntShim(4), Bar::ID, b2, DS_CREATE | DS_EXCL) == DS_OK);
    // cancel the warning one line later than would otherwise because
    // the cache warning doesn't pop up until one command later
    log_notice_p("/test", "flamebox-ignore-cancel ign1");
    CHECK_EQUAL(cache->size(), 64);

    CHECK_EQUAL(cache->count(), 4);
    CHECK_EQUAL(cache->live(), 4);

    // make sure all four objects are in cache
    CHECK(table->get(IntShim(1), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  1);
    CHECK(o == f1);

    CHECK(table->get(IntShim(2), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  2);
    CHECK(o == f2);
    
    CHECK(table->get(IntShim(3), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  3);
    CHECK(o == b1);
    
    CHECK(table->get(IntShim(4), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  4);
    CHECK(o == b2);

    // now release them all in reverse order which will cause b1 and
    // b2 to be evicted
    CHECK(cache->release(IntShim(4), b2) == DS_OK);
    CHECK(cache->release(IntShim(3), b1) == DS_OK);
    CHECK(cache->release(IntShim(2), f2) == DS_OK);
    CHECK(cache->release(IntShim(1), f1) == DS_OK);

    CHECK_EQUAL(cache->size(), 25);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 0);

    // make sure f1 and f2 are still in-cache
    CHECK(table->get(IntShim(1), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  5);
    CHECK(o == f1);

    CHECK(table->get(IntShim(2), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK(o == f2);

    // release f1 and f2, then try to get b1 and b2 which should
    // create new objects, leaving only b1 in cache when we're done
    CHECK(cache->release(IntShim(1), f1) == DS_OK);
    CHECK(cache->release(IntShim(2), f2) == DS_OK);

    CHECK(table->get(IntShim(3), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK_EQUAL(cache->misses(), 1);
    CHECK_EQUALSTR(o->name(), "bar");
    CHECK_EQUALSTR(o->static_name_.c_str(), "test test");
    CHECK_EQUAL(cache->evictions(), 3);
    CHECK(cache->release(IntShim(3), o) == DS_OK);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 0);
    
    CHECK(table->get(IntShim(4), &o) == DS_OK);
    CHECK_EQUAL(cache->hits(),  6);
    CHECK_EQUAL(cache->misses(), 2);
    CHECK_EQUALSTR(o->name(), "bar");
    CHECK_EQUALSTR(o->static_name_.c_str(), "test test test");
    CHECK_EQUAL(cache->evictions(), 5);
    CHECK(cache->release(IntShim(4), o) == DS_OK);
    CHECK_EQUAL(cache->count(), 1);
    CHECK_EQUAL(cache->live(), 0);

    // delete an object out of cache, a live object in cache (can't
    // do), and a dead object in cache
    CHECK(table->get(IntShim(1), &o) == DS_OK);
    CHECK_EQUAL(cache->count(), 2);
    CHECK_EQUAL(cache->live(), 1);
    
    log_notice_p("/test", "flamebox-ignore ign1 .*can't "
               "remove live object .* from cache");
    CHECK(table->del(IntShim(1)) == DS_ERR);
    log_notice_p("/test", "flamebox-ignore-cancel ign1");
    
    CHECK(table->get(IntShim(1), &o) == DS_OK);
    CHECK(cache->release(IntShim(1), o) == DS_OK);
    CHECK(table->del(IntShim(1)) == DS_OK);
    CHECK(table->get(IntShim(1), &o) == DS_NOTFOUND);
    CHECK(table->del(IntShim(2)) == DS_OK);
    CHECK(table->get(IntShim(2), &o) == DS_NOTFOUND);
    CHECK(table->del(IntShim(3)) == DS_OK);
    CHECK(table->get(IntShim(3), &o) == DS_NOTFOUND);
    CHECK(table->del(IntShim(4)) == DS_OK);
    CHECK(table->get(IntShim(4), &o) == DS_NOTFOUND);
    
    CHECK_EQUAL(cache->count(), 0);
    CHECK_EQUAL(cache->live(),  0);
    CHECK_EQUAL(cache->size(),  0);
    
    delete_z(table);
    DEL_DS_STORE(store);
    delete_z(cache);
    
    return UNIT_TEST_PASSED;
}
