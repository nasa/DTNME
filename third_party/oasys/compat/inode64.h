/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _INODE64_H_
#define _INODE64_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../debug/DebugUtils.h"

namespace oasys {

/*
 * On some systems (currently hard coded to be only Mac OS X), an
 * st_ino is only 32 bits wide, but to correctly support NFS we really
 * need 64 bits of uniqueness. However, we can use the st_flags to
 * store the high-order bits and reconstruct it when needed.
 *
 * Thus the following functions should be used whenever
 * storing/getting the st_ino value.
 */
#ifndef __APPLE__
#define INODE64
#endif

inline u_int64_t
get_inode64(const struct stat* st)
{
#ifdef INODE64
    return st->st_ino;
#else
    return (u_int64_t)st->st_ino | (((u_int64_t)st->st_gen) << 32);
#endif
}

inline void
set_inode64(struct stat* st, u_int64_t ino)
{
#ifdef INODE64
    STATIC_ASSERT(sizeof(st->st_ino) == 8, InodeShouldBe64Bits);
    st->st_ino = ino;
#else
    STATIC_ASSERT(sizeof(st->st_ino) == 4, InodeShouldBe32Bits);
    st->st_ino = ino & 0xffffffffLL;
    st->st_gen = (ino >> 32);

    ASSERT(get_inode64(st) == ino);
#endif
}

#undef INODE64

} // namespace oasys


#endif /* _INODE64_H_ */
