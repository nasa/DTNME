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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "RefCountedObject.h"

namespace oasys {

//----------------------------------------------------------------------
RefCountedObject::RefCountedObject(const char* logpath)
    : refcount_(0),
      logger_("RefCountedObject", "%s_b", logpath)
{
}

//----------------------------------------------------------------------
RefCountedObject::~RefCountedObject()
{
}

//----------------------------------------------------------------------
void
RefCountedObject::add_ref(const char* what1, const char* what2) const
{
    atomic_incr(&refcount_);

#ifdef OASYS_LOG_DEBUG_ENABLED
    logger_.logf(LOG_DEBUG,
                 "refcount *%p %u -> %u add %s %s",
                 this, refcount_.value - 1, refcount_.value, what1, what2);
#else
    (void) what1;
    (void) what2;    
#endif
    
    ASSERT(refcount_.value > 0);
}

//----------------------------------------------------------------------
void
RefCountedObject::del_ref(const char* what1, const char* what2) const
{
    ASSERT(refcount_.value > 0);

#ifdef OASYS_LOG_DEBUG_ENABLED
    logger_.logf(LOG_DEBUG,
                 "refcount *%p %d -> %d del %s %s",
                 this, refcount_.value, refcount_.value - 1, what1, what2);
#else
    (void) what1;
    (void) what2;    
#endif
    
    // atomic_decr_test will only return true if the currently
    // executing thread is the one that sent the refcount to zero.
    // hence we are safe in knowing that there are no other references
    // on the object, and that only one thread will call
    // no_more_refs()
    
    if (atomic_decr_test(&refcount_)) {
        ASSERT(refcount_.value == 0);
        no_more_refs();
    }
}

//----------------------------------------------------------------------
void
RefCountedObject::no_more_refs() const
{
#ifdef OASYS_LOG_DEBUG_ENABLED
    logger_.logf(LOG_DEBUG, "no_more_refs *%p... deleting object", this);
#endif

    delete this;
}

//----------------------------------------------------------------------
int
RefCountedObject::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "RefCountedObject %p", this);
}

} // namespace oasys
