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
#include <sys/stat.h>
#include <algorithm>

#include "util/UnitTest.h"
#include "util/STLUtil.h"
#include "storage/FileBackedObject.h"
#include "storage/FileBackedObjectStore.h"
#include "storage/FileBackedObjectStream.h"

using namespace oasys;

FileBackedObjectStore* g_store = 0;

DECLARE_TEST(Init) {
    system("rm -rf testdir");
    system("mkdir testdir");
    g_store = new FileBackedObjectStore("testdir");

    CHECK(g_store != 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Fini) {
    delete g_store;
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(StoreTest) {
    CHECK(! g_store->object_exists("test1"));
    CHECK(g_store->new_object("test1") == 0);
    CHECK(g_store->new_object("test1") != 0);
    CHECK(g_store->object_exists("test1"));
    CHECK(g_store->copy_object("test1", "test2") == 0);
    CHECK(g_store->copy_object("test1", "test2") != 0);
    CHECK(g_store->object_exists("test2"));
    CHECK(g_store->del_object("test1") == 0);
    CHECK(g_store->del_object("test1") != 0);
    CHECK(! g_store->object_exists("test1"));
    CHECK(g_store->del_object("test2") == 0);
    CHECK(g_store->del_object("test2") != 0);
    CHECK(! g_store->object_exists("test2"));

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(FileTest) {
    CHECK(g_store->new_object("test") == 0);

    FileBackedObjectHandle h;
    h = g_store->get_handle("test", 0);
    
    const char* teststr = "hello world";

    CHECK(h->write_bytes(0, reinterpret_cast<const u_char*>(teststr), 
                         strlen(teststr)) == strlen(teststr));

    char buf[256];
    memset(buf, 0, 256);
    CHECK(h->read_bytes(0, reinterpret_cast<u_char*>(buf), 
                        strlen(teststr)) == strlen(teststr));
    CHECK_EQUALSTR(buf, teststr);
    CHECK(h->size() == strlen(teststr));
    h.reset();

    FILE* f = fopen("testdir/test", "r");
    fgets(buf, 256, f);
    CHECK_EQUALSTR(buf, teststr);

    fclose(f);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TXTest) {
    const char* teststr = "hello world";
    const char* teststr2 = "goodbye world";

    FileBackedObjectHandle h;
    h = g_store->get_handle("test", 0);

    FileBackedObject::TxHandle tx = h->start_tx(0);
    tx->object()->write_bytes(0, reinterpret_cast<const u_char*>(teststr2), 
                              strlen(teststr2));

    char buf[256];
    memset(buf, 0, 256);
    CHECK(h->read_bytes(0, reinterpret_cast<u_char*>(buf), strlen(teststr)) == 
          strlen(teststr));
    CHECK_EQUALSTR(buf, teststr);    
    CHECK(h->size() == strlen(teststr));

    // commit the transaction
    tx.reset();

    CHECK(h->read_bytes(0, reinterpret_cast<u_char*>(buf), strlen(teststr2)) == 
          strlen(teststr2));
    CHECK_EQUALSTR(buf, teststr2);    
    CHECK(h->size() == strlen(teststr2));    
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(StreamTest) {
    FileBackedObjectHandle h;
    h = g_store->get_handle("test", 0);
    
    u_char buf[256];
    {
        const char* teststr = "goodbye world";
        FileBackedObjectInStream in(h.get());
        int err = in.read(buf, strlen(teststr));
        CHECK(err == 0);
        CHECK(memcmp(buf, teststr, strlen(teststr)) == 0);
    }

    {
        const char* teststr = "hello world again";
        FileBackedObjectOutStream out(h.get());
        size_t err = out.write(reinterpret_cast<const u_char*>(teststr), strlen(teststr));
        CHECK(err == 0);
        err = h->read_bytes(0, buf, strlen(teststr));
        CHECK(err == strlen(teststr));
        CHECK(memcmp(buf, teststr, strlen(teststr)) == 0);
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AllNamesTest) {
    CHECK(g_store->new_object("all_dir1") == 0);
    CHECK(g_store->new_object("all_dir2") == 0);
    CHECK(g_store->new_object("all_dir3") == 0);
    CHECK(g_store->new_object("all_dir4") == 0);
    CHECK(g_store->new_object("all_dir5") == 0);
    CHECK(g_store->new_object("all_dir6") == 0);
    CHECK(g_store->new_object("all_dir7") == 0);

    std::vector<std::string> v;
    g_store->get_object_names(&v);
    
    std::vector<std::string> c;
    CommaPushBack<std::vector<std::string>, std::string> pusher(&c);
    pusher = pusher, 
             "all_dir1", "all_dir2", "all_dir3", 
             "all_dir4", "all_dir5", "all_dir6", 
             "all_dir7", "test";
    std::sort(v.begin(), v.end());
    
    CHECK(v == c); 

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Init);
    ADD_TEST(StoreTest);
    ADD_TEST(FileTest);
    ADD_TEST(TXTest);
    ADD_TEST(StreamTest);
    ADD_TEST(AllNamesTest);
    ADD_TEST(Fini);
}

DECLARE_TEST_FILE(Test, "sample test");
