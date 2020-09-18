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

#include <string>
#include "storage/MemoryStore.h"

//
// globals needed by the generic durable-store-test
//

#define DEL_DS_STORE(store) delete_z(store) 

using namespace oasys;

//
// pull in the generic test
//
#include "durable-store-test.cc"

DECLARE_TEST(DBTestInit) {
    g_config = new StorageConfig(
        "storage",              // command name
        "memorydb",             // type
        "",	                // dbname
        "" 		        // dbdir
    );   

    g_config->init_             = true;
    g_config->tidy_             = false;
    g_config->leave_clean_file_ = false;
    g_config->tidy_wait_        = 0;

    return 0;
}

DECLARE_TESTER(MemoryStoreTester) {
    ADD_TEST(DBTestInit);

    ADD_TEST(DBInit);
    ADD_TEST(TableCreate);
    ADD_TEST(TableDelete);
    ADD_TEST(TableGetNames);

    ADD_TEST(SingleTypePut);
    ADD_TEST(SingleTypeGet);
    ADD_TEST(SingleTypeDelete);
    ADD_TEST(SingleTypeMultiObject);
    ADD_TEST(SingleTypeIterator);
    ADD_TEST(SingleTypeCache);

    ADD_TEST(NonTypedTable);
    ADD_TEST(MultiType);
    ADD_TEST(MultiTypeCache);
}

DECLARE_TEST_FILE(MemoryStoreTester, "memory store test");
