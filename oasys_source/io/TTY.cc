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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "../compat/cfsetspeed.h"
#include "../compat/cfmakeraw.h"
#include "TTY.h"

namespace oasys {

//----------------------------------------------------------------------
TTY::TTY(const char* logpath)
    : FileIOClient(logpath)
{
    memset(&tcattrs_, 0, sizeof(tcattrs_));
}

//----------------------------------------------------------------------
TTY::~TTY()
{
}

//----------------------------------------------------------------------
char*
TTY::ttyname()
{
    log_debug("ttyname(%d)", fd_);
    return ::ttyname(fd_);
}

//----------------------------------------------------------------------
int
TTY::isatty()
{
    log_debug("isatty(%d)", fd_);
    return ::isatty(fd_);
}

//----------------------------------------------------------------------
speed_t
TTY::cfgetispeed()
{
    log_debug("cfgetispeed(%d)", fd_);
    return ::cfgetispeed(&tcattrs_);
}

//----------------------------------------------------------------------
int
TTY::cfsetispeed(speed_t speed)
{
    log_debug("cfsetispeed(%d, %u)", fd_, static_cast<u_int>(speed));
    return ::cfsetispeed(&tcattrs_, speed);
}


//----------------------------------------------------------------------
speed_t
TTY::cfgetospeed()
{
    log_debug("cfgetospeed(%d)", fd_);
    return ::cfgetospeed(&tcattrs_);
}

//----------------------------------------------------------------------
int
TTY::cfsetospeed(speed_t speed)
{
    log_debug("cfsetospeed(%d, %u)", fd_, static_cast<u_int>(speed));
    return ::cfsetospeed(&tcattrs_, speed);
}


//----------------------------------------------------------------------
int
TTY::cfsetspeed(speed_t speed)
{
    log_debug("cfsetspeed(%d, %u)", fd_, static_cast<u_int>(speed));
    return ::cfsetspeed(&tcattrs_, speed);
}

//----------------------------------------------------------------------
void
TTY::cfmakeraw()
{
    log_debug("cfmakeraw(%d)", fd_);
    ::cfmakeraw(&tcattrs_);
}

//----------------------------------------------------------------------
int
TTY::tcgetattr()
{
    log_debug("tcgetattr(%d)", fd_);
    memset(&tcattrs_, 0, sizeof(tcattrs_));
    return ::tcgetattr(fd_, &tcattrs_);
}

//----------------------------------------------------------------------
int
TTY::tcsetattr(int action)
{
    log_debug("tcsetattr(%d)", fd_);
    return ::tcsetattr(fd_, action, &tcattrs_);
}

//----------------------------------------------------------------------
int
TTY::tcdrain()
{
    log_debug("tcdrain(%d)", fd_);
    return ::tcdrain(fd_);
}

//----------------------------------------------------------------------
int
TTY::tcflow(int action)
{
    log_debug("tcflow(%d, %d)", fd_, action);
    return ::tcflow(fd_, action);
}

//----------------------------------------------------------------------
int
TTY::tcflush(int action)
{
    log_debug("tcflush(%d, %d)", fd_, action);
    return ::tcflush(fd_, action);
}

//----------------------------------------------------------------------
int
TTY::tcsendbreak(int len)
{
    log_debug("tcsendbreak(%d, %d)", fd_, len);
    return ::tcsendbreak(fd_, len);
}

} // namespace oasys
