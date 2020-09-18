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
#include <string.h>
#include <util/MD5.h>

using namespace oasys;

// works like md5sum
int
main(int argc, const char** argv)
{
    char buf[256];
    MD5 md5;
    
    while (1) {
        char* line = fgets(buf, sizeof(buf), stdin);
        if (!line)
            break;
        md5.update((u_char*)line, strlen(line));
    }

    md5.finalize();
    std::string digest = md5.digest_ascii();
    
    printf("%s\n", digest.c_str());
}
