/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _OASYS_TTY_H_
#define _OASYS_TTY_H_

#include <termios.h>
#include "FileIOClient.h"

namespace oasys {

/**
 * Wrapper class for terminal devices.
 */
class TTY : public FileIOClient {
public:
    TTY(const char* logpath = "/oasys/io/tty");
    virtual ~TTY();

    /// @{ Logging wrappers around tty-related functions
    char*   ttyname();
    int     isatty();
    speed_t cfgetispeed();
    int     cfsetispeed(speed_t speed);
    speed_t cfgetospeed();
    int     cfsetospeed(speed_t speed);
    int     cfsetspeed(speed_t speed);
    void    cfmakeraw();
    int     tcgetattr();
    int     tcsetattr(int action);
    int     tcdrain();
    int     tcflow(int action);
    int     tcflush(int action);
    int     tcsendbreak(int len);
    /// @}

    /// Accessor to the cached terminal attributes
    struct termios& tcattrs() { return tcattrs_; }

protected:
    struct termios tcattrs_;
};

} // namespace oasys 

#endif /* _OASYS_TTY_H_ */
