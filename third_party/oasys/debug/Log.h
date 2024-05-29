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


#ifndef _OASYS_LOG_H_
#define _OASYS_LOG_H_

/**
 * @class oasys::Log
 *  
 * Dynamic Log system implementation.
 *
 * Basic idea is that a logf command contains an arbitrary log path, a
 * level, and a printf-style body. For example:
 *
 * @code
 * logf("/some/path", LOG_INFO, "something happened %d times", count);
 * @endcode
 *
 * Each application should at intialization call Log::init(level) to
 * prime the default level. All log messages with a level equal or
 * higher than the default are output, others aren't. All output goes
 * to stdout.
 *
 * The code also checks for a ~/.debug file which can optionally
 * contain log paths and other levels. For example:
 *
 * @code
 * # my ~/.debug file
 * % brief color
 * /some/path debug
 * /other info
 * /other/more/specific debug
 * @endcode
 *
 * The paths are interpreted to include everything below them, thus in
 * the example above, all messages to /some/path/... would be output at
 * level debug.
 *
 * In addition to the basic logf interface, there are also a set of
 * macros (log_debug(), log_info(), log_notice(), log_warn(),
 * log_err(), log_crit()) that are more efficient.
 * 
 * For example, log_debug expands to
 *
 * @code
 * #define log_debug(path, ...)                         \
 *    if (log_enabled(LOG_DEBUG, path)) {               \
 *        logf(LOG_DEBUG, path, ## __VA_ARGS__);        \
 *   }                                                  \
 * @endcode
 *
 * In this way, the check for whether or not the path is enabled at
 * level debug occurs before the call to logf(). As such, the
 * formatting of the log string, including any inline calls in the arg
 * list are avoided unless the log line will actually be output.
 *
 * In addition, under non-debug builds, all calls to log_debug are
 * completely compiled out.
 *
 * .debug file options:
 * 
 * There are several options that can be used to customize the display
 * of debug output, and these options are specified on a line in the
 * .debug file prefixed with '%' (see example above):
 *
 * no_path   - Don't display log path<br>
 * no_time - Don't display time stamp<br>
 * no_level  - Don't display log level<br>
 * brief     - Truncate log name to a fixed length and use brief error codes<br>
 * color   - Use terminal escape code to colorize output<br>
 * object  - When possible, display the address of the object that
 *           generated the log.<br>
 * classname - When possible, display the class that generated the log message.<br>
 * walltime  - If time is displayed, format it as HH:MM:SS.mmm instead of the
 *             default SSSSS.uuuuuu
 */

#include <cstdarg>
#include <cstdio>
#include <iostream> /* see http://mailman.dtnrg.org/pipermail/dtn-users/2007-January/000448.html */
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <string>
#include <vector>
#include <strings.h>
#include <sys/uio.h>

#if defined(__GNUC__)
# define PRINTFLIKE(fmt, arg) __attribute__((format (printf, fmt, arg)))
#else
# define PRINTFLIKE(a, b)
#endif

namespace oasys {

#define LOG_DEFAULT_THRESHOLD oasys::LOG_INFO
#define LOG_DEFAULT_DBGFILE   "~/.debug"

#define LOG_MAX_PATHLEN (64)
#define LOG_MAX_LINELEN (512)

typedef enum {
    LOG_INVALID = -1,
    LOG_DEBUG   = 1,
    LOG_INFO    = 2,
    LOG_NOTICE  = 3,
    LOG_WARN    = 4,
    LOG_ERR     = 5,
    LOG_CRIT    = 6,
    LOG_ALWAYS  = 7
} log_level_t;

#ifndef DOXYGEN
struct level2str_t {
    const char* str;
    log_level_t level;
};

extern level2str_t log_levelnames[];

#endif

inline const char *level2str(log_level_t level) {
    for (int i = 0; log_levelnames[i].str != 0; i++) {
        if (log_levelnames[i].level == level) {
            return log_levelnames[i].str;
        }
    }
    
    return "(unknown level)";
}

inline log_level_t str2level(const char *level)
{
    for (int i = 0; log_levelnames[i].str && i < 20; i++) {
        if (strcasecmp(log_levelnames[i].str, level) == 0) {
            return log_levelnames[i].level;
        }
    }

    return LOG_INVALID;
}

void __log_assert(bool x, const char* what, const char* file, int line);

// From vfprintf.c
extern "C" int log_vsnprintf(char *str, size_t strsz, const char *fmt0, va_list ap);
extern "C" int log_snprintf(char *str, size_t strsz, const char *fmt, ...);

class SpinLock;
class StringBuffer;

class Log {
public:
    /**
     * Singleton instance accessor. Note that Log::init must be called
     * before this function, hence static initializers can't use the
     * logging system.
     */
    static Log *instance() {
        __log_assert((inited_ == true),
                     "Log::init not called yet",
                     __FILE__, __LINE__);

        return instance_; 
    }

    /**
     * Initialize the logging system. Must be called exactly once.
     */
    static void init(const char* logfile    = "-",
                     log_level_t defaultlvl = LOG_DEFAULT_THRESHOLD,
                     const char *prefix     = NULL,
                     const char *debug_path = LOG_DEFAULT_DBGFILE);

    /**
     * Initialize the logging system. Must be called exactly once.
     */
    static void init(log_level_t defaultlvl)
    {
        init("-", defaultlvl);
    }

    /**
     * Hook to determine if logging is initialized yet.
     */
    static bool initialized()
    {
        return inited_;
    }



    /**
     * Change the default log level on the fly
     */
    bool set_loglevel(log_level_t defaultlvl);

    /**
     * Shut down the logging system.
     */
    static void shutdown();

    /**
     * @brief Basic unformatted logging function.
     *
     * @param[in] path the log path to use (used in the log entry
     * prefix and for checking path-specific log levels)
     *
     * @param[in] level the importance/severity of the message
     *
     * @param[in] classname name of the class of the object that is
     * logging (used for checking classname-specific log levels).  Can
     * be NULL if not applicable.
     *
     * @param[in] obj Pointer to print in the log entry prefix (if
     * that feature is enabled).  May be NULL if not applicable.
     *
     * @param[in] msg the message to log
     *
     * @param[in] prefix_each_line whether to add a prefix to each line
     * in @p msg (like log_multiline())
     *
     * @return the number of bytes written to the log file
     */
    int log(const std::string& path, log_level_t level,
            const char* classname, const void* obj,
            const std::string& msg, bool prefix_each_line = false);

    /**
     * @brief Core logging function that is the guts of the
     * implementation of other variants.
     *
     * The length of a single log entry (including the prefix
     * generated by gen_prefix()) can be as long as #LOG_MAX_LINELEN
     * without causing this function to truncate the log entry.  If
     * the log entry does not fit in #LOG_MAX_LINELEN bytes, then the
     * log entry will be truncated to fit within #LOG_MAX_LINELEN and
     * the text "... (truncated)" will appear at the end of the log
     * entry (assuming #LOG_MAX_LINELEN is long enough to hold the
     * text "... (truncated)").
     *
     * Note that if a trailing newline is not produced by applying @p
     * ap to @p fmt, one will be appended automatically.  Also, if @p
     * path does not begin with a forward slash ('/'), then one will
     * be prepended.
     *
     * @return the number of bytes written (e.g., zero if the log line
     * was suppressed) if there was no error, -1 if there was an error
     */
    int vlogf(const char *path, log_level_t level,
              const char* classname, const void* obj,
              const char *fmt, va_list ap);

    /**
     * Alternative core log function that expects a multi-line,
     * preformatted log buffer. Generates a single prefix that is
     * repeated for each line of output.
     */
    int log_multiline(const char* path, log_level_t level, 
                      const char* classname, const void* obj,
                      const char* msg);

    /**
     * Return the log level currently enabled for the path / class.
     */
    log_level_t log_level(const char *path);

    /**
     * Parse the debug file and repopulate the rule list. Called from
     * init or from an external handler to reparse the file. If
     * debug_path is unspecified, it defaults to the existing file.
     */
    void parse_debug_file(const char* debug_path = NULL);

    /**
     * Set the logging prefix after initialization.
     */
    void set_prefix(const char* prefix)
    {
        prefix_.assign(prefix);
    }

    /**
     * Close and reopen the log file.
     */
    void rotate();

    /**
     * Set up a signal handler for the given signal to do log
     * rotation.
     */
    void add_rotate_handler(int sig);

    /**
     * Set up a signal handler for the given signal to re-parse the
     * debug file.
     */
    void add_reparse_handler(int sig);

    /**
     * Debugging function to print the rules list.
     */
    void dump_rules(StringBuffer* buf);

    /**
     * Redirect stdout/stderr to the logging file descriptor by using
     * dup2. Note that if done once at startup time, this is repeated
     * whenever the log file is rotated.
     */
    void redirect_stdio();

    /**
     * Debugging hook used for the log unit test to test out the
     * overflow guard without actually triggering a panic.
     */
    static bool __debug_no_panic_on_overflow;

protected:
    friend class LogCommand;
    
    Log();

    /**
     * Initialize logging, should be called exactly once from the
     * static Log::init or LogSim::init.
     */
    void do_init(const char* logfile, log_level_t defaultlvl,
                 const char* prefix, const char* debug_path);

    /**
     * Singleton instance of the Logging system
     */
    static Log* instance_;

    /**
     * @brief Outputs @p data to the log file
     *
     * @param[in] iov Pointer to a vector of data/len iovec pairs.
     *
     * @param[in] iovcnt Number of iovecs.
     *
     * @return the number of bytes written to the output
     */
    int output(const struct iovec* iov, int iovcnt);

private:
    /**
     * Structure used to store a log rule as parsed from the debug
     * file.
     */
    struct Rule {
        Rule(const char* path, log_level_t level)
            : path_(path), level_(level) {}

        Rule(const Rule& rule)
            : path_(rule.path_), level_(rule.level_) {}
        
        std::string path_;
        log_level_t level_;
    };


    /**
     * Shutdown implementation.
     */
    void fini();

    /**
     * Sorting function for log rules. The rules are stored in
     * descending length order, so a linear scan through the list will
     * always return the most specific match. For equal-length rules,
     * the lower-level (i.e. more permissive) rule should be first so
     * for equal paths, the more permissive rule wins.
     */
    static bool rule_compare(const Rule& rule1, const Rule& rule2);

    /**
     * Use a vector for the list of rules.
     */
    typedef std::vector<Rule> RuleList;
    
    /**
     * Output format types
     */
    enum {
        OUTPUT_PATH      = 1<<0,   // output the log path 
        OUTPUT_TIME      = 1<<1,   // output time in logs
        OUTPUT_LEVEL     = 1<<2,   // output level in logs
        OUTPUT_CLASSNAME = 1<<3,   // output the class name generating the log
        OUTPUT_OBJ       = 1<<4,   // output the obj generating the log
        OUTPUT_SHORT     = 1<<10,  // shorten prefix
        OUTPUT_COLOR     = 1<<11,  // colorific
        OUTPUT_WALLTIME  = 1<<12,  // output time as HH:MM:SS.mmm
    };

    /**
     * Output control flags
     */
    int output_flags_;

    /**
     * Generate the log prefix.
     *
     * @param[out] buf the buffer to fill with the log entry prefix.
     * This may be the null pointer if @p buflen is 0.  If @p buflen
     * is non-zero, @p buf will always be null-terminated -- even if
     * it is not large enough to hold the whole prefix.
     *
     * @param[in] buflen no more than this number of characters will
     * be written to @p buf (including the trailing null character).
     * This may be zero.  If zero, @p buf may be the null pointer.
     *
     * @param[in] path the log path to use (used in the log entry
     * prefix and for checking path-specific log levels)
     *
     * @param[in] level the importance/severity of the message
     *
     * @param[in] classname name of the class of the object that is
     * logging (used for checking classname-specific log levels).  Can
     * be NULL if not applicable.
     *
     * @param[in] obj Pointer to print in the log entry prefix (if
     * that feature is enabled).  May be NULL if not applicable.
     *
     * @return If @p buflen is big enough to hold the whole log entry
     * prefix (including the null terminating character), this returns
     * the number of characters written (not including the null
     * terminator).  If @p buflen is NOT big enough to hold the whole
     * log entry prefix (including the null terminator), then this
     * returns the number of characters that would have been written
     * (not including the null terminator) had @p buflen been big
     * enough.
     */
    size_t gen_prefix(char* buf, size_t buflen,
                      const char* path, log_level_t level,
                      const char* classname, const void* obj) const;

    /**
     * Find a rule given a path.
     */
    Rule *find_rule(const char *path);

    static bool inited_;	///< Flag to ensure one-time intialization
    static bool shutdown_;	///< Flag to mark whether we've shut down
    std::string logfile_;	///< Log output file (- for stdout)
    int logfd_;			///< Output file descriptor
    bool stdio_redirected_;	///< Flag to redirect std{out,err}
    RuleList* rule_list_;	///< Pointer to current list of logging rules
    RuleList  rule_lists_[2];	///< Double-buffered rule lists for reparsing
    SpinLock* output_lock_;	///< Lock for write calls and rotating
    std::string debug_path_;    ///< Path to the debug file
    std::string prefix_;	///< String to prefix log messages
    log_level_t default_threshold_; ///< The default threshold for log messages
};

/**
 * Global vlogf function.
 */
inline int
vlogf(const char *path, log_level_t level, 
      const char *fmt, va_list ap)
{
    if (path == 0) { return -1; } // XXX/bowei arghh..
    return Log::instance()->vlogf(path, level, NULL, NULL, fmt, ap);
}

/**
 * Global logf function.
 */
inline int
logf(const char *path, log_level_t level, const char *fmt, ...)
    PRINTFLIKE(3, 4);

inline int
logf(const char *path, log_level_t level, const char *fmt, ...)
{
    if (!path) return -1;
    va_list ap;
    va_start(ap, fmt);
    int ret = Log::instance()->vlogf(path, level, NULL, NULL, fmt, ap);
    va_end(ap);
    return ret;
}

/**
 * Global log_multiline function.
 */
inline int
log_multiline(const char* path, log_level_t level, const char* msg)
{
    return Log::instance()->log_multiline(path, level, NULL, NULL, msg);
}

/**
 * @brief Global log function.
 *
 * @param[in] path the log path to use (used in the log entry prefix
 * and for checking path-specific log levels)
 *
 * @param[in] level the importance/severity of the message
 *
 * @param[in] msg the message to log
 *
 * @param[in] prefix_each_line whether to add a prefix to each line in
 * @p msg (like log_multiline())
 *
 * @return the number of bytes written to the log file
 */
inline int
log(const std::string& path, log_level_t level, const std::string& msg,
    bool prefix_each_line = false)
{
    return Log::instance()->log(path, level, NULL, NULL, msg, prefix_each_line);
}

/**
 * Global function to determine if the log path is enabled. Overridden
 * by the Logger class.
 */
inline bool
log_enabled(log_level_t level, const char* path)
{
    log_level_t threshold = oasys::Log::instance()->log_level(path);
    return (level >= threshold);
}

} // namespace oasys

/**
 * The set of macros below are implemented for more efficient
 * implementation of logging functions. As noted in the comment above,
 * these macros first check whether logging is enabled on the path and
 * then call the output formatter. As such, all output string
 * formatting and argument calculations are only done if the log path
 * is enabled.
 *
 * Since most users of logging are a subclass of Logger, the
 * log_debug() style macros assume that they are being called in a
 * method of a Logger class instance. To assist users with more
 * informative error messages, these macros refer to the
 * Can_Only_Be_Called_By_A_Logger typedef that won't be in scope for
 * any non-Logger contexts.
 *
 * The log_debug_p() variant should be used in global contexts.
 */

// compile out all log_debug calls when not debugging
#ifdef NDEBUG
inline int log_nop() { return 0; }
#define log_debug(args...)   log_nop()
#define log_debug_p(args...) log_nop()
#else
#    ifdef OASYS_LOG_DEBUG_ENABLED
        #define log_debug(args...)                                                      \
             (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_DEBUG) ?     \
              this->logf(oasys::LOG_DEBUG, ## args) : 0)

        #define log_debug_p(p, args...)                         \
            ((oasys::log_enabled(oasys::LOG_DEBUG, (p))) ?      \
             oasys::logf((p), oasys::LOG_DEBUG, ## args) : 0)
#    else
        inline int log_nop() { return 0; }
        #define log_debug(args...)   log_nop()
        #define log_debug_p(args...) log_nop()
#    endif // OASYS_LOG_DEBUG_ENABLED
#endif // NDEBUG

#define log_info(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_INFO) ?       \
      this->logf(oasys::LOG_INFO, ## args) : 0)

#define log_info_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_INFO, (p))) ?                      \
     oasys::logf((p), oasys::LOG_INFO, ## args) : 0)

#define log_notice(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_NOTICE) ?       \
      this->logf(oasys::LOG_NOTICE, ## args) : 0)

#define log_notice_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_NOTICE, (p))) ?                      \
     oasys::logf((p), oasys::LOG_NOTICE, ## args) : 0)

#define log_warn(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_WARN) ?       \
      this->logf(oasys::LOG_WARN, ## args) : 0)

#define log_warn_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_WARN, (p))) ?                      \
     oasys::logf((p), oasys::LOG_WARN, ## args) : 0)

#define log_err(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_ERR) ?       \
      this->logf(oasys::LOG_ERR, ## args) : 0)

#define log_err_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_ERR, (p))) ?                      \
     oasys::logf((p), oasys::LOG_ERR, ## args) : 0)

#define log_crit(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_CRIT) ?       \
      this->logf(oasys::LOG_CRIT, ## args) : 0)

#define log_crit_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_CRIT, (p))) ?                      \
     oasys::logf((p), oasys::LOG_CRIT, ## args) : 0)

#define log_always(args...)                                                        \
     (this->log_enabled((Can_Only_Be_Called_By_A_Logger)oasys::LOG_ALWAYS) ?       \
      this->logf(oasys::LOG_ALWAYS, ## args) : 0)

#define log_always_p(p, args...)                                 \
    ((oasys::log_enabled(oasys::LOG_ALWAYS, (p))) ?                      \
     oasys::logf((p), oasys::LOG_ALWAYS, ## args) : 0)


// Include Logger.h for simplicity.
#include "Logger.h"

#endif /* _OASYS_LOG_H_ */
