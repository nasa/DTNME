/*
 *    Copyright 2007 Baylor University
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
#  include <config.h>
#endif

#include <util/UnitTest.h>
#include <util/OptParser.h>
#include <bluez/Bluetooth.h>
#include <bluez/BluetoothInquiry.h>

using namespace oasys;

DECLARE_TEST(BluezInquiry) {
    BluetoothInquiry inq;
    log_always_p("/test",
            "Starting up Bluetooth inquiry");
    int num = inq.inquire();
    CHECK( num > 0 );
    bdaddr_t remote;
    while (inq.next(remote) != -1) {
        num--;
        log_always_p("/test", 
                "Found %s",bd2str(remote));
    }
    CHECK( num == 0 );
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(BluezInquiryTest) {
    ADD_TEST(BluezInquiry);
}

DECLARE_TEST_FILE(BluezInquiryTest, "bluetooth inquiry test");
