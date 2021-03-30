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
#include <oasys-config.h>
#endif

#include <unistd.h>
#include <sys/types.h>

#include "util/ExpandableBuffer.h"
#include "util/UnitTest.h"
#include "storage/FileBackedObject.h"
#include "storage/CheckedLog.h"

using namespace oasys;

const char* str[] = {
    "hello, world",
    "another record",
    "last record"
};

DECLARE_TEST(WriteTest) {
    system("rm -f check-log-test.fbo");
    system("touch check-log-test.fbo");
    
    int fd = open("check-log-test.fbo", 
                  O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
    FdIOClient obj(fd);

    CheckedLogWriter wr(&obj);
    
    wr.write_record(str[0], strlen(str[0]));
    wr.write_record(str[1], strlen(str[1]));
    wr.write_record(str[2], strlen(str[2]));
    close(fd);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ReadTest) {

    int fd = open("check-log-test.fbo", O_RDWR | O_CREAT, S_IRWXU);
    FdIOClient obj(fd);

    struct stat stat_buf;
    fstat(fd, &stat_buf);
    
    ftruncate(fd, stat_buf.st_size - 2);

    CheckedLogReader rd(&obj);

    int ret;
    ExpandableBuffer buf;

    ret = rd.read_record(&buf);
    CHECK(ret == 0);
    CHECK(memcmp(buf.raw_buf(), str[0], strlen(str[0])) == 0);

    ret = rd.read_record(&buf);
    CHECK(memcmp(buf.raw_buf(), str[1], strlen(str[1])) == 0);

    ret = rd.read_record(&buf);
    CHECK(ret == CheckedLogReader::BAD_CRC);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Cleanup) {
    system("rm check-log-test.fbo");

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(WriteTest);
    ADD_TEST(ReadTest);
    ADD_TEST(Cleanup);
}

DECLARE_TEST_FILE(Test, "checked log test");
