/* -*-C++-*-
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


#ifndef _OASYS_LOGGER_H_
#define _OASYS_LOGGER_H_

#include "DebugUtils.h"
#include "Log.h"

namespace oasys {

/**
 * Many objects will, at constructor time, format a log target and
 * then use it throughout the code implementation -- the Logger class
 * encapsulates this common behavior.
 *
 * It therefore exports a set of functions (set_logpath, logpathf,
 * logpath_appendf) to manipulate the logging path post-construction.
 *
 * To support logging on a class basis instead of on a hierarchical
 * basis, the Logger also takes the name of the class in the
 * constructor. Since, by convention, paths start with '/' and classes
 * cannot, this allows independent logging control by class name or by
 * logging path.
 *
 * For example:
 *
 * @code
 * class LoggerTest : public Logger {
 * public:
 *   LoggerTest() : Logger("LoggerTest", "/logger/test")
 *   {
 *       logf(LOG_DEBUG, "initializing"); 		 // no path needed
 *       logf("/logger/test" LOG_DEBUG, "intiializing"); // but can be given
 *
 *       log_debug("loggertest initializing");           // macros work
 *       log_debug_p("/logger/test", "initializing");    // but this case needs
 *                                                       // to be log_debug_p
 *       set_logpath("/path");
 *       logpath_appendf("/a"); // logpath is "/path/a"
 *       logpath_appendf("/b"); // logpath is "/path/b"
 *   }
 * };
 * @endcode
 */

class Logger {
public:    
    /**
     * Constructor that initializes the logpath with a printf style
     * format string.
     *
     * @param classname Constant class name string. WARNING: the
     * string contents are not copied, so the caller must ensure that
     * it remains valid for the lifetime of the Logger object.
     *
     * @param fmt Format string to format the log path.
     */
    inline Logger(const char* classname, const char* fmt, ...) PRINTFLIKE(3, 4);

    /**
     * Constructor that initializes to a constant std::string.
     *
     * @param classname Constant class name string. WARNING: the
     * string contents are not copied, so the caller must ensure that
     * it remains valid for the lifetime of the Logger object.
     *
     * @param logpath The logpath string.
     */
    Logger(const char* classname, const std::string& logpath)
        : classname_(classname)
    {
        set_logpath(logpath.c_str());
    }

    /**
     * @brief Format function for logpath_.
     *
     * A forward slash ('/') is automatically prepended to the logpath
     * if the result of applying @p ap to @p fmt does not begin with
     * '/'.
     */
    void vlogpathf(const char* fmt, va_list ap);

    /**
     * @brief Format function for logpath_.
     *
     * A forward slash ('/') is automatically prepended to the logpath
     * if the result of applying the arguments to @p fmt does not
     * begin with '/'.
     */
    inline void logpathf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Format function that appends to the end of the base path
     * instead of overwriting.
     *
     * For example:
     *
     * \code
     *   set_logpath("/path");
     *   logpath_appendf("/a"); // logpath is "/path/a"
     *   logpath_appendf("/b"); // logpath is "/path/b"
     * \endcode
     */
    inline void logpath_appendf(const char* fmt, ...) PRINTFLIKE(2, 3);
    
    /**
     * Assignment function.
     */
    void set_logpath(const char* logpath)
    {
        this->logpathf("%s", logpath);
    }

    /**
     * Wrapper around the base __log_enabled function that uses the
     * logpath_ instance.
     *
     * Also, all Logger instances store the class name of the
     * implementation, and logging can be enabled/disabled on that
     * target as well, so we check the class name as well.
     */
    inline bool log_enabled(log_level_t level) const
    {
        return oasys::log_enabled(level, logpath_) ||
            oasys::log_enabled(level, classname_);
    }

    /**
     * Typedef used in the log_debug() style macros to help provide
     * more useful error messages.
     */
    typedef log_level_t Can_Only_Be_Called_By_A_Logger;
    
    /**
     * Wrapper around vlogf that uses the logpath_ instance.
     */
    inline int vlogf(log_level_t level, const char *fmt, va_list args) const
    {
        return Log::instance()->vlogf(logpath_, level, classname_, this,
                                      fmt, args);
    }

    /**
     * Wrapper around logf that uses the logpath_ instance.
     */
    inline int logf(log_level_t level, const char *fmt, ...) const
        PRINTFLIKE(3, 4);

    /**
     * Yet another wrapper that just passes the log path straight
     * through, ignoring the logpath_ instance altogether. This means
     * a derived class doesn't need to explicitly qualify ::logf.
     */
    inline int logf(const char* logpath, log_level_t level,
                    const char* fmt, ...) const
        PRINTFLIKE(4, 5);

    /**
     * And finally, one for log_multiline..
     */
    inline int log_multiline(log_level_t level, const char* msg) const
    {
        return Log::instance()->log_multiline(logpath_, level, 
                                              classname_, this, msg);
    }

    /**
     * @brief Logs a message without formatting.
     */
    int log(log_level_t level, const std::string& message,
            bool prefix_each_line = false) const;

    /**
     * @return current logpath
     */
    const char* logpath() const { return logpath_; }

protected:
    const char* classname_;
    char logpath_[LOG_MAX_PATHLEN];
    size_t baselen_;
};

//----------------------------------------------------------------------
Logger::Logger(const char* classname, const char* fmt, ...)
    : classname_(classname)
{
    va_list ap;
    va_start(ap, fmt);
    this->vlogpathf(fmt, ap);
    va_end(ap);
}

//----------------------------------------------------------------------
inline void
Logger::vlogpathf(const char* fmt, va_list ap)
{
    if (fmt[0] == '/')
    {
        // handle the common case where fmt starts with '/'
        log_vsnprintf(logpath_, sizeof(logpath_), fmt, ap);
    }
    else
    {
        // print to a temporary buffer
        char tmp[LOG_MAX_PATHLEN];
        log_vsnprintf(tmp, sizeof(tmp), fmt, ap);

        // determine whether the temporary buffer begins with a '/'
        fmt = "%s";
        if (tmp[0] != '/')
            fmt = "/%s";

        // copy the temporary buffer to its final location, prepending
        // with a '/' as appropriate
        snprintf(logpath_, sizeof(logpath_), fmt, tmp);
    }
    
    // update the length of the logpath_
    baselen_ = strlen(logpath_);
}

//----------------------------------------------------------------------
void
Logger::logpathf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    this->vlogpathf(fmt, ap);
    va_end(ap);
}

//----------------------------------------------------------------------
void
Logger::logpath_appendf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_vsnprintf(&logpath_[baselen_], sizeof(logpath_) - baselen_, fmt, ap);
    va_end(ap);
    // baselen_ is not updated here on purpose
}

//----------------------------------------------------------------------
int
Logger::logf(log_level_t level, const char *fmt, ...) const
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vlogf(level, fmt, ap);
    va_end(ap);
    return ret;
}

//----------------------------------------------------------------------
int
Logger::logf(const char* logpath, log_level_t level,
             const char *fmt, ...) const
{
    va_list ap;
    va_start(ap, fmt);
    int ret = Log::instance()->vlogf(logpath, level, classname_, this,
                                     fmt, ap);
    va_end(ap);
    return ret;
}

//----------------------------------------------------------------------
inline int
Logger::log(log_level_t level, const std::string& message,
            bool prefix_each_line) const
{
    return Log::instance()->log(logpath_, level, classname_, this,
                                message, prefix_each_line);
}


} // namespace oasys

#endif /* _OASYS_LOGGER_H_ */
