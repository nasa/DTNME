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
#  include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>

#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include <oasys/thread/SpinLock.h>
#include "storage/BundleStore.h"
#include "storage/DTNStorageConfig.h"

using namespace dtn;
using namespace oasys;

int
payload_test(BundlePayload::location_t location)
{
    u_char buf[64];
    const u_char* data;
    SpinLock l;
    BundlePayload p(&l);
    bzero(buf, sizeof(buf));

    int errno_; const char* strerror_;

    log_debug_p("/test", "checking initialization");
    p.init(1, location);
    CHECK_EQUAL(p.location(), location);
    CHECK(p.length() == 0);
    
    log_debug_p("/test", "checking set_data");
    p.set_data((const u_char*)"abcdefghijklmnopqrstuvwxyz", 26);
    CHECK_EQUAL(p.location(), location);
    CHECK_EQUAL(p.length(), 26);
    
    log_debug_p("/test", "checking read_data");
    data = p.read_data(0, 26, buf);
    CHECK_EQUALSTR((char*)data, "abcdefghijklmnopqrstuvwxyz");
    data = p.read_data(10, 5, buf);
    CHECK_EQUALSTRN((char*)data, "klmno", 5);
    
    log_debug_p("/test", "checking truncate");
    p.truncate(10);
    data = p.read_data(0, 10, buf);
    CHECK_EQUAL(p.length(), 10);
    CHECK_EQUALSTRN((char*)data, "abcdefghij", 10);

    log_debug_p("/test", "checking append_data");
    p.append_data((u_char*)"ABCDE", 5);
    data = p.read_data(0, 15, buf);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDE", 15);

    p.append_data((u_char*)"FGH", 3);
    data = p.read_data(0, 18, buf);
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "abcdefghijABCDEFGH", 18);
    
    log_debug_p("/test", "checking write_data overwrite");
    p.write_data((u_char*)"BCD", 1, 3);
    data = p.read_data(0, 18, buf);
    CHECK_EQUAL(p.length(), 18);
    CHECK_EQUALSTRN((char*)data, "aBCDefghijABCDEFGH", 18);

    log_debug_p("/test", "checking write_data with gap");
    p.set_length(30);
    p.write_data((u_char*)"XXXXX", 25, 5);
    CHECK_EQUAL(p.length(), 30);

    p.write_data((u_char*)"_______", 18, 7);
    data = p.read_data(0, 30, buf);
    CHECK_EQUAL(p.length(), 30);
    CHECK_EQUALSTR((char*)data, "aBCDefghijABCDEFGH_______XXXXX");

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MemoryPayloadTest) {
    return payload_test(BundlePayload::MEMORY);
}

DECLARE_TEST(DiskPayloadTest) {
    return payload_test(BundlePayload::DISK);
}

DECLARE_TESTER(BundlePayloadTester) {
    ADD_TEST(MemoryPayloadTest);
    ADD_TEST(DiskPayloadTest);
}

int
main(int argc, const char** argv)
{
    BundlePayloadTester t("bundle payload test");
    t.init(argc, argv, true);
    
    system("rm -rf .bundle-payload-test");
    system("mkdir  .bundle-payload-test");
    DTNStorageConfig cfg("", "memorydb", "", "");
    cfg.init_ = true;
    cfg.payload_dir_.assign(".bundle-payload-test");
    cfg.leave_clean_file_ = false;
    oasys::DurableStore ds("/test/ds");
    ds.create_store(cfg);
    BundleStore::init(cfg, &ds);
    
    t.run_tests();

    system("rm -rf .bundle-payload-test");
}
