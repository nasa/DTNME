/*
 *    Copyright 2005-2006 Intel Corporation
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

#include "ProgressPrinter.h"
#include "../thread/Timer.h"

namespace oasys {

/**
 * Initialize the progress meter.
 */
ProgressPrinter::ProgressPrinter(const char* fmt, ...)
{
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        fflush(stdout);
    }
    
    ::gettimeofday(&start_, 0);
}

/**
 * Restart the progress meter.
 */
void
ProgressPrinter::start(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    
    ::gettimeofday(&start_, 0);
}

/**
 * Print the "done" message, along with the elapsed time, plus any
 * additional info.
 */
void
ProgressPrinter::done(const char* fmt, ...)
{
    struct timeval end;

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    ::gettimeofday(&end, 0);
    unsigned long elapsed = TIMEVAL_DIFF_MSEC(end, start_);
    
    printf(" (%lu.%.3lu secs)\n", elapsed / 1000, (elapsed % 1000));
}

} // namespace oasys

