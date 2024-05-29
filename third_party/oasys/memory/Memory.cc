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
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include "Memory.h"

#ifdef OASYS_DEBUG_MEMORY_ENABLED

namespace oasys {

#define _BYTE char
#define _DBG_MEM_MAGIC      0xf00dbeef

int              DbgMemInfo::entries_   = 0;
dbg_mem_entry_t* DbgMemInfo::table_     = 0;
bool             DbgMemInfo::init_      = false;
int              DbgMemInfo::dump_file_ = -1;
struct sigaction DbgMemInfo::signal_;

void
DbgMemInfo::init(   
    int   flags,
    char* dump_file
    )
{
    // XXX/bowei Needs to be changed to MMAP
    entries_ = 0;
    table_   = (dbg_mem_entry_t*)
        calloc(_DBG_MEM_TABLE_SIZE, sizeof(dbg_mem_entry_t));
    memset(table_, 0, sizeof(dbg_mem_entry_t) * _DBG_MEM_TABLE_SIZE);

    if(flags & DbgMemInfo::USE_SIGNAL) 
    {

        memset(&signal_, 0, sizeof(struct sigaction));
        signal_.sa_sigaction = DbgMemInfo::signal_handler;
        //signal_.sa_mask    = 
        signal_.sa_flags     = SA_SIGINFO;
        
        ::sigaction(SIGUSR2, &signal_, 0);
    }
    
    if(dump_file) 
    {
        dump_file_ = open(dump_file, 
                          O_WRONLY | O_CREAT | O_APPEND);
    }

    init_ = true;
}


void
DbgMemInfo::debug_dump(bool only_diffs)
{
    for(int i=0; i<_DBG_MEM_TABLE_SIZE; ++i) 
    {
        dbg_mem_entry_t* entry = &table_[i];
        if (entry->frames_[0] == 0)
            continue;

        if (! only_diffs || (entry->live_ != entry->last_live_)) {
            log_info("/memory", "%5d: [%p %p %p] live=%d last_live=%d size=%.2fkb\n",
                     i,
                     entry->frames_[0],
                     entry->frames_[1],
                     entry->frames_[2],
                     entry->live_,
                     entry->last_live_,
                     (float)entry->size_/1000);
        }

        entry->last_live_ = entry->live_;
    }
}

void
DbgMemInfo::dump_to_file(int fd)
{
    if(fd == -1) {
        return;
    }

    struct timeval time;
    char buf[256];

    gettimeofday(&time, 0);
    ctime_r((const time_t*)&time.tv_sec, buf);
    write(fd, buf, strlen(buf));

    for(int i=0; i<_DBG_MEM_TABLE_SIZE; ++i)
    {
        dbg_mem_entry_t* entry = &table_[i];
        if(entry->frames_[0] == 0)
            continue;
        
        snprintf(buf, 256,
                 "%5d: [%p %p %p] live=%d size=%.2fkb\n",
                 i,
                 entry->frames_[0],
                 entry->frames_[1],
                 entry->frames_[2],
                 entry->live_,
                 (float)entry->size_/1000);
        
        write(fd, buf, strlen(buf));
    }
    fsync(fd);
}

void
DbgMemInfo::signal_handler(
    int        signal, 
    siginfo_t* info, 
    void*      context
    )
{
    dump_to_file(dump_file_);
}

} // namespace oasys

/** 
 * Put the previous stack frame information into frames
 */
static inline void 
set_frame_info(void** frames)
{
#ifdef __GNUC__
#define FILL_FRAME(_x)                                  \
    if(__builtin_frame_address(_x) == 0) {              \
        return;                                         \
    } else {                                            \
        frames[_x-1] = __builtin_return_address(_x);    \
    }

    FILL_FRAME(1);
    FILL_FRAME(2);
    FILL_FRAME(3);
    FILL_FRAME(4);
#undef FILL_FRAME
#else
#error Depends on compiler implementation, implement me.
#endif
}

void* 
operator new(size_t size) throw (std::bad_alloc)
{
    // The reason for these two code paths is the prescence of static
    // initializers which allocate memory on the heap. Memory
    // allocated before init is called is not tracked.
    oasys::dbg_mem_t* b = static_cast<oasys::dbg_mem_t*>
                          (malloc(sizeof(oasys::dbg_mem_t) + size));

    if(b == 0) {
        throw std::bad_alloc();
    }
    
    memset(b, 0, sizeof(oasys::dbg_mem_t));
    b->magic_ = _DBG_MEM_MAGIC;
    b->size_  = size;

    // non-init allocations have frame == 0
    if (oasys::DbgMemInfo::initialized()) {
        void* frames[_DBG_MEM_FRAMES];
        
        set_frame_info(frames);
        b->entry_ = oasys::DbgMemInfo::inc(frames, size);

// logging may not be initialized yet...
        
//      log_debug("/memory", "new a=%p, f=[%p %p %p]\n",              
//                &b->block_, frames[0], frames[1], frames[2]);     
    }
                                                                
    return (void*)&b->block_;                               
}

void
operator delete(void *ptr) throw ()
{
    oasys::dbg_mem_t* b = PARENT_PTR(ptr, oasys::dbg_mem_t, block_);

    ASSERT(b->magic_ == _DBG_MEM_MAGIC);

    if (b->entry_ != 0) {
// logging may not be initialized yet...
        
//      log_debug("/memory", "delete a=%p, f=[%p %p %p]\n", 
//                &b->block_, 
//                b->entry_->frames_[0], b->entry_->frames_[1], 
//                b->entry_->frames_[2]);

        oasys::DbgMemInfo::dec(b);
    }
    
    char* bp = (char*)(b);
    unsigned int size = b->size_;

    for(unsigned int i=0; i<size; ++i)
    {
        bp[i] = 0xF0;
    }

    free(b);
}

#endif // OASYS_DEBUG_MEMORY_ENABLED

