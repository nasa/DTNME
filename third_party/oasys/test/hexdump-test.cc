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

#include <errno.h>
#include "io/FileIOClient.h"
#include "io/FileUtils.h"
#include "util/HexDumpBuffer.h"

using namespace oasys;

int
main(int argc, const char** argv)
{
    Log::init();
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        exit(1);
    }

    const char* file = argv[1];

    if (!FileUtils::readable(file)) {
        fprintf(stderr, "can't read file %s\n", file);
        exit(1);
    }

    FileIOClient f;
    if (f.open(file, O_RDONLY | O_EXCL) < 0) {
        fprintf(stderr, "error opening file: %s\n", strerror(errno));
        exit(1);
    }

    int size = FileUtils::size(file);
    HexDumpBuffer h(size);
    h.append(&f, size);
    f.close();

    h.hexify();
    printf("%s", h.c_str());
}

     
    
