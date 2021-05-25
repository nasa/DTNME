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

#ifndef _PROGRESSPRINTER_H_
#define _PROGRESSPRINTER_H_

#include <sys/time.h>

#include "../debug/Log.h" // for PRINTFLIKE macro

namespace oasys {

class ProgressPrinter {
public:
    /**
     * Initialize the progress meter.
     */
    ProgressPrinter(const char* fmt = NULL, ...);

    /**
     * Restart the progress meter.
     */
    void start(const char* fmt, ...);

    /**
     * Print the "done" message, along with the elapsed time, plus any
     * additional info.
     */
    void done(const char* fmt = " done", ...) PRINTFLIKE(2, 3);

protected:
    struct timeval start_;
};

} // namespace oasys

#endif /* _PROGRESSPRINTER_H_ */
