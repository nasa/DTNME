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
#include <bluez/BluetoothSDP.h>

using namespace oasys;

unsigned int sleep_seconds = 30;

DECLARE_TEST(SDPRegistration) {
    log_always_p("/test",
            "holding SDP registration open for %u seconds",sleep_seconds);
    BluetoothServiceRegistration sdpreg("oasys bluez test");
    CHECK(sdpreg.success());
    sleep(sleep_seconds);
    log_always_p("/test",
            "removing registration");
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(SDPTest) {
    ADD_TEST(SDPRegistration);
}

//DECLARE_TEST_FILE(SDPTest, "bluetooth sdp registration test");

int main(int argc, const char* argv[]) {

    OptParser p;
    const char* invalid = NULL;

    p.addopt(new UIntOpt("sleep",&sleep_seconds,"how long to hold SDP "
             "registration open"));

    if (!p.parse(argc-1,argv+1,&invalid)) {
        fprintf(stderr,"Bad parameter %s\n",invalid);
        exit(1);
    }

    RUN_TESTER(SDPTest,"bluetooth SDP registration test",
               argc-1,argv);
}
