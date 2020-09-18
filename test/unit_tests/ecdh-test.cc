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

#include <stdio.h>
#include <string>

#include <oasys/util/UnitTest.h>
#include "security/ECDH.h"

using namespace dtn;
using namespace oasys;

typedef oasys::ScratchBuffer<u_char*, 16> LocalBuffer;

u_char buf[64];

bool
test(int sec_lev)
{
    unsigned char *cms = NULL;
    size_t cms_len;
    unsigned char *dec = NULL;
    size_t dec_len;
    FILE *dumpfile= NULL;
    ERR_load_crypto_strings();
    char certfile[1024];
    char privfile[1024];
    sprintf(certfile, "cert%d.pem", sec_lev);
    sprintf(privfile, "priv%d.pem", sec_lev);
    LocalBuffer input;
    LocalBuffer output;
    LocalBuffer output2;
    input.reserve(11);
    input.set_len(11);
    memcpy(input.buf(), (unsigned char *)"Hi there!\n",11);

    if(ECDH::encrypt(input, certfile, output,sec_lev) < 0) {
        ERR_print_errors_fp(stderr);
        return UNIT_TEST_FAILED;
    }
    printf("Finished encrypt\n");
    dumpfile = fopen("cmsdump", "w");
    fwrite(output.buf(), sizeof( char), output.len(), dumpfile);
    fclose(dumpfile);
    printf("Wrote to file\n");


    if(ECDH::decrypt(output2, privfile, output) < 0) {
         ERR_print_errors_fp(stderr);
         return UNIT_TEST_FAILED;
    }
    printf("Got back: %s", (char *)output2.buf());
    if(strcmp("Hi there!\n", (char *)output2.buf()) == 0) {
        return UNIT_TEST_PASSED;
    }

}

DECLARE_TEST(ECDH) {
    // a few random value tests
    //CHECK(test(128) == UNIT_TEST_PASSED);
    CHECK(test(192) == UNIT_TEST_PASSED);
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ECDHTest) {
    ADD_TEST(ECDH);
}

DECLARE_TEST_FILE(ECDHTest, "ECDH test");
