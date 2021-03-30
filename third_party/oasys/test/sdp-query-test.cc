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
#include <bluez/BluetoothSDP.h>

using namespace oasys;

// global scope so that main can set it for test routine
bdaddr_t remote = {{ 0, 0, 0, 0, 0, 0 }};

DECLARE_TEST(SDPQuery) {
    BluetoothServiceDiscoveryClient sdpclient;
    log_always_p("/test",
            "Attempting to query %s for DTN UUID",bd2str(remote));
    CHECK(sdpclient.is_dtn_router(remote));
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(SDPQueryTest) {
    ADD_TEST(SDPQuery);
}

//DECLARE_TEST_FILE(SDPQueryTest, "bluetooth sdp query test");

int main(int argc, const char* argv[]) {

    OptParser p;
    const char* invalid = NULL;
    bool isset = false; // check that remote gets set

    p.addopt(new BdAddrOpt("remote",&remote,"Bluetooth adapter address "
             "of SDP server","",&isset));

    if (!p.parse(argc-1,argv+1,&invalid) || !isset) {
        fprintf(stderr,"must set \"remote=btaddr\" where \"btaddr\" is "
                "Bluetooth adapter address of SDP server\n");
        exit(1);
    }

    RUN_TESTER(SDPQueryTest,"bluetooth SDP query test",
               argc-1,argv);
}
