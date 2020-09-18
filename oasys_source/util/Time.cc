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

/*
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <math.h>

#include "Time.h"

#if defined(_WIN32)

#include "compat/MSVC.h"

#else

#include <sys/time.h>
#include <time.h>

#endif

#include "debug/DebugUtils.h"

namespace oasys {

//----------------------------------------------------------------------
Time::Time(double d)
{
    sec_  = static_cast<u_int32_t>(floor(d));

    // we add a fraction of a microsecond before converting to prevent
    // the microseconds part from being 999999 in some cases of
    // conversion to and from the integer values
    usec_ = static_cast<u_int32_t>((d - floor(d) + 0.0000001) * 1000000);

    cleanup();
}

//----------------------------------------------------------------------------
void
Time::get_time() 
{
#if defined(_WIN32)

    SYSTEMTIME systime;
    FILETIME   filetime;

    GetSystemTime(&systime);
    SystemTimeToFileTime(&systime, &filetime);
    
    // FileTime is in 100-nanosecond intervals since UTC January 1, 1601
    // 116444736000000000 0.1 usec difference
    // 11644473600 sec difference
    __int64 ft;
    ft = filetime.dwLowDateTime;
    ft |= ((__int64)filetime.dwHighDateTime)<<32;

    ft -= 116444736000000000;

    // Get microseconds
    ft /= 10; 
    usec_ = static_cast<u_int32_t>(ft % 1000000);

    // Get seconds
    ft /= 1000000;
    sec_ = static_cast<u_int32_t>(ft);

#else
    struct timeval tv;

    gettimeofday(&tv, 0);
    sec_  = static_cast<u_int32_t>(tv.tv_sec);
    usec_ = static_cast<u_int32_t>(tv.tv_usec);

#endif

    cleanup();
}

//----------------------------------------------------------------------------
double 
Time::in_seconds() const
{
    return static_cast<double>(sec_) + 
        static_cast<double>(usec_)/1000000;
}

//----------------------------------------------------------------------------
u_int32_t
Time::in_microseconds() const
{
    return sec_ * 1000000 + usec_;
}
    
//----------------------------------------------------------------------------
u_int32_t
Time::in_milliseconds() const
{
    return sec_ * 1000 + usec_/1000;
}

//----------------------------------------------------------------------
u_int32_t
Time::elapsed_ms() const
{
    Time t;
    t.get_time();
    
    // jward - I keep seeing operator-= blow an assert because t is somehow
    // less than the original time (bug in linux's gettimeofday()?). We'll
    // just assume that means not much (==0) time has passed.
    if (t <= *this )
        return 0;
    
    t -= *this;
    return t.in_milliseconds();
}

//----------------------------------------------------------------------------
Time
Time::operator+(const Time& t) const
{
    Time ret(t.sec_ + sec_, t.usec_ + usec_);
    ASSERT(ret >= t); // check overflow
    return ret;
}

//----------------------------------------------------------------------------
Time&
Time::operator+=(const Time& t)
{
    sec_  += t.sec_;
    usec_ += t.usec_;
    cleanup();
    ASSERT(*this >= t); // check overflow
    return *this;
}

//----------------------------------------------------------------------------
Time&
Time::operator-=(const Time& t)
{
    // a precondition for this fn to be correct is (*this >= t)
    //XXX/dz - Several instances where successive calls to time 
    //         can return the same time so changing to return
    //         zero if t >= this
    //ASSERT(*this >= t);
    if (t >= *this) {
        sec_  = 0;
        usec_ = 0;
    } else {
        if (usec_ < t.usec_) {
            usec_ += 1000000;
            sec_  -= 1;
        }

        sec_  -= t.sec_;
        usec_ -= t.usec_;
    }

    return *this;
}

//----------------------------------------------------------------------------
Time
Time::operator-(const Time& t) const
{
    // a precondition for this fn to be correct is (*this >= t)
    //XXX/dz - see above operator-    ASSERT(*this >= t);
    
    Time t2(*this);
    t2 -= t;
    return t2;
}

//----------------------------------------------------------------------------
bool
Time::operator==(const Time& t) const
{
    return (sec_ == t.sec_) && (usec_ == t.usec_);
}

//----------------------------------------------------------------------------
bool
Time::operator<(const Time& t) const
{
    return (sec_ < t.sec_) 
        || ( (sec_ == t.sec_) && (usec_ < t.usec_));
}

//----------------------------------------------------------------------------
bool
Time::operator>(const Time& t) const
{
    return (sec_ > t.sec_) 
        || ( (sec_ == t.sec_) && (usec_ > t.usec_));    
}

//----------------------------------------------------------------------------
bool
Time::operator<=(const Time& t) const
{
    return (*this == t) || (*this < t);
}

//----------------------------------------------------------------------------
bool
Time::operator>=(const Time& t) const
{
    return (*this == t) || (*this > t);
}
 
//----------------------------------------------------------------------
void
Time::add_seconds(u_int32_t secs)
{
    ASSERT(sec_ + secs >= secs); // check overflow
    sec_ += secs;
}

//----------------------------------------------------------------------
void
Time::add_milliseconds(u_int32_t msecs)
{
    sec_  += msecs / 1000;
    usec_ += (msecs % 1000) * 1000;
    cleanup();
    ASSERT(in_milliseconds() >= msecs); // check overflow
}
        
//----------------------------------------------------------------------
void
Time::add_microseconds(u_int32_t usecs)
{
    sec_  += usecs / 1000000;
    usec_ += usecs % 1000000;
    cleanup();
    ASSERT(in_microseconds() >= usecs); // check overflow
}

//----------------------------------------------------------------------------
void
Time::cleanup()
{
    if (usec_ > 1000000) 
    {
        sec_ += usec_ / 1000000;
        usec_ /= 1000000;
    }
}

//----------------------------------------------------------------------------
TimeScope::TimeScope(log_level_t level, const char* path, const char* comment)
    : level_(level), path_(path), comment_(comment)
{
    start_.get_time();
}

//----------------------------------------------------------------------------
TimeScope::~TimeScope()
{
    logf(path_, level_, "%s took %u milliseconds", 
         comment_, start_.elapsed_ms());
}

//----------------------------------------------------------------------------
TimeSection::Scope::Scope(TimeSection* section)
    : section_(section)
{
    start_.get_time();
}

//----------------------------------------------------------------------------
TimeSection::Scope::~Scope()
{
    section_->ms_ += start_.elapsed_ms();
    ++section_->invoked_;
}

//----------------------------------------------------------------------------
TimeSection::TimeSection()
    : ms_(0), invoked_(0)
{}

} // namespace oasys

#if 0

#include <cstdio>

int 
main()
{
    oasys::Time t;

    t.get_time();

    printf("%d %d\n", t.sec_, t.usec_);

    return 0;
} 

#endif
