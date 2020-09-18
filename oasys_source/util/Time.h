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

#ifndef __TIME_H__
#define __TIME_H__

#include "../compat/inttypes.h"
#include "../debug/Log.h"

namespace oasys {

/*!
 * Structure to handle time in a platform independent way.
 */
struct Time {
    u_int32_t sec_;
    u_int32_t usec_;

    Time(u_int32_t sec  = 0,
         u_int32_t usec = 0) 
        : sec_(sec), usec_(usec) 
    {
        cleanup();
    }

    Time(double d);

    //! Return a Time structure containing the current time
    static Time now()
    {
        Time t;
        t.get_time();
        return t;
    }
    
    //! Get the time into the structure
    void get_time();
    
    //! @return Time in seconds as a floating point number
    double in_seconds() const;

    //! @return Time in microseconds
    u_int32_t in_microseconds() const;
    
    //! @return Time in milliseconds
    u_int32_t in_milliseconds() const;

    //! @return Milliseconds elapsed between this timer and the
    // current time.
    u_int32_t elapsed_ms() const;

    //! @{ Standard operators
    Time operator+(const Time& t)  const;
    Time operator-(const Time& t)  const;
    Time& operator+=(const Time& t);
    Time& operator-=(const Time& t);

    bool operator==(const Time& t) const;
    bool operator<(const Time& t)  const;
    bool operator>(const Time& t)  const;
    bool operator>=(const Time& t) const;
    bool operator<=(const Time& t) const;
    //! @}

    // Use default operator=

    //! @{ Add various time units.
    void add_seconds(u_int32_t secs);
    void add_milliseconds(u_int32_t msecs);
    void add_microseconds(u_int32_t usecs);
    //! @}

    //! Cleanup the usec field wrt. sec
    void cleanup();
};

/*!
 * Print a debug message of the time elapsed between creation and
 * destruction.
 */
class TimeScope {
public:
    TimeScope(log_level_t level, const char* path, const char* comment);
    ~TimeScope();

private:
    Time start_;
    log_level_t level_;
    const char* path_;
    const char* comment_;
};

/*!
 * Useful for timing a section which takes too short of a time with
 * each invocation but aggregately is slow.
 */
class TimeSection {
public:
    struct Scope {
        Scope(TimeSection* section);
        ~Scope();
        
        Time         start_;
        TimeSection* section_;
    };
    friend struct Scope;

    TimeSection();
    
    int ms() { return ms_; }
    int invoked() { return invoked_; }
    
private:
    int ms_;
    int invoked_;
};

} // namespace oasys

#endif /* __TIME_H__ */
