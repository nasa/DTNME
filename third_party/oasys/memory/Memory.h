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

#ifndef _OASYS_MEMORY_H_
#define _OASYS_MEMORY_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_DEBUG_MEMORY_ENABLED

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <signal.h>
#include <sys/mman.h>

#include "../compat/inttypes.h"
#include "../debug/DebugUtils.h"
#include "../debug/Log.h"
#include "../util/jenkins_hash.h"

/** 
 * Regular new call. Untyped allocations have type _UNKNOWN_TYPE.
 */
void* operator new(size_t size) throw (std::bad_alloc);

/**
 * Delete operator. If the memory frame info is 0, then this memory
 * allocation is ignored.
 */ 
void operator delete(void *ptr) throw ();

namespace oasys {

/**
 * @file Debug memory allocator that keep track of different object
 * types and other memory usage accounting statistics. 
 *
 * Implementation note: When the tracking hash table gets near 95%
 * full, new will stop adding entries into the hash table, and a
 * warning will be signaled.
 *
 * Optionally, the allocator can be configured so that when it
 * received a user signal, it dumps the current memory use information
 * to dump file.
 *
 * Options:
 *
 * Define DEBUG_NEW_THREAD_SAFE at compile time to have thread safe
 * statistics at some loss of performance. Otherwise, there will be
 * occasional races in updating the number of live objects.
 *
 * Define _DBG_MEM_TABLE_EXP to be power of 2 size of the number of
 * memory entries.
 * 
 * The region of memory in which the debugging information is stored
 * is mmapped to a high address which (hopefully) should not intersect
 * with the addresses in standard usage. The reason for this kind of
 * implementation is that the use of the debug malloc does not change
 * the malloc allocation pattern (e.g. malloc doesn't behave
 * differently with/out the debug malloc stuff.
 *
 */

#ifdef __GNUC__
// Align the memory block that are allocated to correct (and bus error free).
#define _ALIGNED __attribute__((aligned))
#else
#error Must define aligned attribute for this compiler.
#endif 

#define _BYTE               char
#define _DBG_MEM_FRAMES     3

#ifndef _DBG_MEM_TABLE_EXP
#define _DBG_MEM_TABLE_EXP  10
#endif 

#define _DBG_MEM_TABLE_SIZE 1<<_DBG_MEM_TABLE_EXP
#define _DBG_MEM_MMAP_HIGH  

/** 
 * Get the containing structure given the address of _ptr->_field.
 */ 
#define PARENT_PTR(_ptr, _type, _field)                 \
    ( (_type*) ((_BYTE*)_ptr - offsetof(_type, _field)) )

/**
 * An entry in the memory block information table.
 */ 
struct dbg_mem_entry_t {
    void* frames_[_DBG_MEM_FRAMES]; ///< # of stack frames to snarf in LIFO order
    int       live_;       ///< Objects of this type that are alive.
    int       last_live_;  ///< Objects of this type that were alive at the last dump.
    u_int32_t size_;       ///< Size of objects
};

/** 
 * Allocated memory block layout will store an entry to the
 * dbg_mem_entry_t of the type of the allocation.
 */
struct dbg_mem_t {
    unsigned long    magic_;
    dbg_mem_entry_t* entry_; 
    u_int32_t        size_;
    
    _BYTE            block_ _ALIGNED; ///< actual memory block
};

/**
 * Memory usage statistics class. YOU MUST CALL DbgMemInfo::init() at
 * the start of the program or else none of this will work.
 */ 
class DbgMemInfo {
public:
    enum {
        NO_FLAGS   = 0,
        USE_SIGNAL = 1,   // set up a signal handler for dumping memory information
    };

    
    /**
     * Set up the memory usage statistics table.
     *
     * @param dump_file Dump of the memory usage characteristics.
     */
    static void init(int flags = NO_FLAGS, char* dump_file = 0);

// Find a matching set of frames
#define MATCH(_f1, _f2)                                                 \
    (memcmp((_f1), (_f2), sizeof(void*) * _DBG_MEM_FRAMES) == 0)

// Mod by a power of 2
#define MOD(_x, _m)                                             \
    ((_x) & ((unsigned int)(~0))>>((sizeof(int)*8) - (_m)))

    /** 
     * Find the memory block.
     */
    static inline dbg_mem_entry_t* find(void** frames) {
        int key = MOD(jenkins_hash((u_int8_t*)frames, 
                                   sizeof(void*) * _DBG_MEM_FRAMES, 0), 
                      _DBG_MEM_TABLE_EXP);
        dbg_mem_entry_t* entry = &table_[key];
    
        // XXX/bowei - may want to do quadratic hashing later if things
        // get too clustered.
        while(entry->frames_[0] != 0 &&
              !MATCH(frames, entry->frames_))
        {
            ++key;
            entry = &table_[key];
        }

        return entry;
    }

    /**
     * Increment the memory info.
     */ 
    static inline dbg_mem_entry_t* inc(
        void**    frames,
        u_int32_t size
        )
    {
        dbg_mem_entry_t* entry = find(frames);

        if(entry->frames_[0] == 0) {
            memcpy(entry->frames_, frames, sizeof(void*) * _DBG_MEM_FRAMES);
            entry->live_      = 1;
            entry->last_live_ = 0;
        } else {
            ++(entry->live_);
        }

        entry->size_ += size;
        ++entries_;

        return entry;
    }

    /**
     * Decrement the memory info.
     */ 
    static inline dbg_mem_entry_t* dec(dbg_mem_t* mem) {
        void**    frames = mem->entry_->frames_;
        u_int32_t size   = mem->size_;

        dbg_mem_entry_t* entry = find(frames);
        
        if(entry->frames_[0] == 0) {
            PANIC("Decrementing memory entry with no frame info");
        } else {
            entry->live_ -= 1;
            entry->size_ -= size;

            if(entry->live_ < 0) {
                PANIC("Memory object live count < 0");
            }
        }

        return entry;
    }

    /**
     * Get the memory attribute table.
     */
    static dbg_mem_entry_t** get_table() { return &table_; }

    /**
     * Dump out debugging information. If alloc_diffs is false, only
     * print the differences since the last time debug_dump was
     * called.
     */
    static void debug_dump(bool only_diffs = false);

    /**
     * Dump out memory usage summary to file.
     *
     * @param fd File to output to.
     */
    static void dump_to_file(int fd);

    /**
     * Getter for init state
     */
    static bool initialized() { return init_; }

    /**
     * Signal handler for dump file
     */
    static void signal_handler(int signal, siginfo_t* info, void* context);

private:
    /** 
     * Pointer to the allocated memory hash table.
     */
    static int              entries_;
    static dbg_mem_entry_t* table_;
    static bool             init_;
    static int              dump_file_;
    static struct sigaction signal_;
};

// clean up namespace
#undef _ALIGNED
#undef _BYTE

#undef MATCH
#undef MOD

} // namespace oasys

#endif // OASYS_DEBUG_MEMORY_ENABLED

#endif //_OASYS_MEMORY_H_
