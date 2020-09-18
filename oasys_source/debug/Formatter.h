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


#ifndef _OASYS_FORMATTER_H_
#define _OASYS_FORMATTER_H_

#include <stdio.h>
#include <stdarg.h>

#include "DebugDumpBuf.h"
#include "DebugUtils.h"
#include "StackTrace.h"

namespace oasys {

/**
 * This class is intended to be used with a modified implementation of
 * snprintf/vsnprintf, defined in Formatter.cc.
 *
 *********************************************************************
 * WARNING!!!  USERS BEWARE
 *
 * Because of the horrible hack used to implement this which goes back
 * and forth between C++ and C and then back to C++...
 * ANY CLASS WHICH INHERITS FROM THIS CLASS **MUST** DECLARE THIS CLASS
 * FIRST IN ITS INHERITANCE LIST.
 *
 * If this is done, a pointer to a class instance will look like a
 * pointer to a Formatter data structure when accessed from C.
 *
 * If this is not adhered to, the cast to Formatter* in
 * formatter_format will not work and generally the test for
 * FORMAT_MAGIC will fail.
 *********************************************************************
 *
 * The modification implements a special control code combination of
 * '*%p' that triggers a call to Formatter::format(), called on the
 * object pointer passed in the vararg list.
 *
 * For example:
 *
 * @code
 * class FormatterTest : public Formatter {
 * public:
 *     virtual int format(char* buf, size_t sz) {
 *         return snprintf(buf, sz, "FormatterTest");
 *     }
 * };
 *
 * FormatterTest f;
 * char buf[256];
 * snprintf(buf, sizeof(buf), "pointer %p, format *%p\n");
 * // buf contains "pointer 0xabcd1234, format FormatterTest\n"
 * @endcode
 */
class Formatter {
public:
    /**
     * Virtual callback, called from this vsnprintf implementation
     * whenever it encounters a format string of the form "*%p".
     *
     * The output routine must not write more than sz bytes and is not
     * null terminated.
     *
     * @return The number of bytes written, or the number of bytes
     * that would have been written if the output is
     * truncated. 
     *
     * XXX/bowei -- this contract is fairly annoying to implement.
     */
    virtual int format(char* buf, size_t sz) const = 0;

    /**
     * Assertion check to make sure that obj is a valid formatter.
     * This basically just makes sure that in any multiple inheritance
     * situation where each base class has virtual functions,
     * Formatter _must_ be the first class in the inheritance chain.
     */
    static inline int assert_valid(const Formatter* obj);

    /**
     * Print out to a statically allocated buffer which can be called
     * from gdb. Note: must not be inlined in order for gdb to be able
     * to execute this function.
     */
    int debug_dump();

    virtual ~Formatter() {}

#ifndef NDEBUG
#define FORMAT_MAGIC 0xffeeeedd
    Formatter() : format_magic_(FORMAT_MAGIC) {}
    Formatter(const Formatter&) : format_magic_(FORMAT_MAGIC) {}
    unsigned int format_magic_;
#else
    Formatter() {}
    Formatter(const Formatter&) {}
#endif // NDEBUG

    
};

void __log_assert(bool x, const char* what, const char* file, int line);

inline int
Formatter::assert_valid(const Formatter* obj)
{
    (void)obj; // avoid unused variable warning

#ifndef NDEBUG
    // Making the program barf is too extreme for Formatter, which is
    // usually just used in logging output.
    if (obj->format_magic_ != FORMAT_MAGIC) 
    {
        fprintf(stderr,
                "Formatter object invalid -- check that Formatter is "
                "the *first* class in inheritance list of class using"
		"the Formatter::format function!!");
        StackTrace::print_current_trace(false);
        oasys_break();

        return 0;
    }
#endif

    return 1;
}

} // namespace oasys

#endif /* _OASYS_FORMATTER_H_ */
