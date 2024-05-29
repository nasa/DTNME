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

#include <vector>

#include <io/IOClient.h>
#include <io/BufferedIO.h>

using namespace oasys;

/*
struct TestClient : public IOClient {
    TestClient() : read_num_(0) {}

    int readv(const struct iovec* iov, int iovcnt)            { return 0; }
    int readall(char* bp, size_t len)                         { return 0; }
    int readvall(const struct iovec* iov, int iovcnt)         { return 0; }
    int writev(const struct iovec* iov, int iovcnt)           { return 0; }
    int writeall(const char* bp, size_t len)                  { return 0; }
    int writevall(const struct iovec* iov, int iovcnt)        { return 0; }
    int timeout_read(char* bp, size_t len, int timeout_ms)    { return 0; }
    int timeout_readall(char* bp, size_t len, int timeout_ms) { return 0; }
    int timeout_readv(const struct iovec* iov, int iovcnt,
                      int timeout_ms) { return 0; }
    int timeout_readvall(const struct iovec* iov, int iovcnt, 
                         int timeout_ms) { return 0; }
    int set_nonblocking(bool b)  { return 0; }
    int get_nonblocking(bool* b) { return 0; }


    int read(char* bp, size_t len) {
        return 0;
    }
    int write(const char* bp, size_t len) { 
        return 0;
    }

    static int    amount_[] = { 3, 1, 7, 4000, 2, 529, 10120, 1, 0 };
    ByteGenerator byte_gen_;
};
*/

// DECLARE_UNIT_TEST(ATest) {
//     return 0;
// }

// DECLARE_UNIT_TEST(AnotherTest) {
//     return UNIT_TEST_FAILED;
// }

// DECLARE_UNIT_TEST(InputTest) {
//     return UNIT_TEST_INPUT;
// }

// class Test : public UnitTester {
//     DECLARE_TESTER(Test);

//     void add_tests() {
//         ADD_UNIT_TEST(ATest);
//         ADD_UNIT_TEST(AnotherTest);
//         ADD_UNIT_TEST(InputTest);
//     }
// };

// DECLARE_TEST_FILE(Test, "BufferedIO");

int main() { Log::init(); NOTIMPLEMENTED; }

// """ # ----------------------------------------------------------------------
