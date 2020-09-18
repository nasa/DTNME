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


#ifndef _OASYS_DEBUG_UTILS_H_
#define _OASYS_DEBUG_UTILS_H_

#include <cstdio>
#include <cstdlib>

#include "FatalSignals.h"

#define ASSERT(x)                                                       \
    do { if (! (x)) {                                                   \
        fprintf(stderr, "ASSERTION FAILED (%s) at %s:%d\n",             \
                (#x), __FILE__, __LINE__);                              \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } } while(0);

#ifdef __GNUC__

#define ASSERTF(x, fmt, args...)                                        \
    do { if (! (x)) {                                                   \
        fprintf(stderr,                                                 \
                "ASSERTION FAILED (%s) at %s:%d: " fmt "\n",            \
                (#x), __FILE__, __LINE__, ## args);                     \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } } while(0);

#define PANIC(fmt, args...)                                             \
    do {                                                                \
        fprintf(stderr, "PANIC at %s:%d: " fmt "\n",                    \
                __FILE__, __LINE__ , ## args);                          \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } while(0);

#endif // __GNUC__

#ifdef __win32__

#define ASSERTF(x, fmt, ...)                                            \
    do { if (! (x)) {                                                   \
        fprintf(stderr,                                                 \
                "ASSERTION FAILED (" #x ") at %s:%d: " fmt "\n",        \
                __FILE__, __LINE__, __VA_ARGS__ );                      \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } } while(0);

#define PANIC(fmt, ...)                                                 \
    do {                                                                \
        fprintf(stderr, "PANIC at %s:%d: " fmt "\n",                    \
                __FILE__, __LINE__ , __VA_ARGS__);                      \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } while(0);

#endif /* __win32__ */

#define NOTREACHED                                                      \
    do {                                                                \
        fprintf(stderr, "NOTREACHED REACHED at %s:%d\n",                \
                __FILE__, __LINE__);                                    \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } while(0);

#define NOTIMPLEMENTED                                                  \
    do {                                                                \
        fprintf(stderr, "%s NOT IMPLEMENTED at %s:%d\n",                \
                __FUNCTION__, __FILE__, __LINE__);                      \
        ::oasys::Breaker::break_here();                                 \
        ::oasys::FatalSignals::die();                                   \
    } while(0);

/** @{ 
 * Compile time static checking (better version, from Boost)
 * Take the sizeof() of an undefined template, which will die.
 */
template <int x> struct static_assert_test{};
template <bool>  struct STATIC_ASSERTION_FAILURE;
template <>      struct STATIC_ASSERTION_FAILURE<true>{};

#ifdef STATIC_ASSERT
#undef STATIC_ASSERT
#endif

#define STATIC_ASSERT(_x, _what)                   \
    typedef static_assert_test                     \
    <                                              \
        sizeof(STATIC_ASSERTION_FAILURE<(bool)(_x)>) \
    > static_assert_typedef_ ## _what;
/** @} */

namespace oasys {
class Breaker {
public:
    /** 
     * This is a convenience method for setting a breakpoint here, in
     * case the stack is completely corrupted by the time the program
     * hits abort(). (Seems completely bogus, but it happens.)
     */
    static void break_here();
};

/*!
 * Make sure to null out after deleting an object. USE THIS.
 */
#define delete_z(_obj)                          \
    do { delete _obj; _obj = 0; } while (0)

/*!
 * @{ Macros to define but not declare copy and assignment operator to
 * avoid accidently usage of such constructs. Put this at the top of
 * the class declaration.
 */
#define NO_COPY(_Classname)                                     \
    private: _Classname(const _Classname& other)

#define NO_ASSIGN(_Classname)                                   \
    private: _Classname& operator=(const _Classname& other)

#define NO_ASSIGN_COPY(_Classname)              \
   NO_COPY(_Classname);                         \
   NO_ASSIGN(_Classname)
//! @}

} // namespace oasys

//! This is outside because it's annoying to type such a long name in
//! gdb
void oasys_break();

//! This as well for dumping formattable objects.
const char* oasys_dump(const void* obj);

//! Dump out a serializable object using the serialization routines.
const char* oasys_sdump(const void* obj);

#endif /* _OASYS_DEBUG_UTILS_H_ */
