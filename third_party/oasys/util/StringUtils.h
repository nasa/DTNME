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


#ifndef _OASYS_STRING_UTILS_H_
#define _OASYS_STRING_UTILS_H_

/**
 * Utilities and stl typedefs for basic_string.
 */

#include <cctype>
#include <cstring>
#include <climits>
#include <vector>
#include <set>
#include <string>
#include <map>
#include <sys/types.h>

#if __cplusplus < 201103L
/* No C++11 support detected. Use deprecated classes instead.
 * NOTE: The headers below probably contain #warning directives.
 */

#warning c++ version is less than 201103L __cplusplus


#include <ext/hash_set>
#include <ext/hash_map>

#define _std __gnu_cxx
#define unordered_set hash_set
#define unordered_map hash_map

#else
/* We have C++11 support, use the standard classes instead
 */
#include <unordered_set>
#include <unordered_map>

#define _std std

// For std::hash 
#include <functional>

#endif

namespace oasys {

//----------------------------------------------------------------------------
/**
 * Hashing function class for std::strings.
 */
#if __cplusplus < 201103L
struct StringHash {
    size_t operator()(const std::string& str) const
    {
        return _std::__stl_hash_string(str.c_str());
    }
};
#else
// C++11 provides us with std::hash class which implements hashing
// so just inherit from it.
struct StringHash : public std::hash<std::string> {};

#endif

//----------------------------------------------------------------------------
/**
 * Less than function.
 */
struct StringLessThan {
    bool operator()(const std::string& str1, const std::string& str2) const
    {
        return (str1.compare(str2) < 0);
    }
};

//----------------------------------------------------------------------------
/**
 * Greater than function.
 */
struct StringGreaterThan {
    bool operator()(const std::string& str1, const std::string& str2) const
    {
        return (str1.compare(str2) > 0);
    }
};

//----------------------------------------------------------------------------
/**
 * Equality function class for std::strings.
 */
struct StringEquals {
    bool operator()(const std::string& str1, const std::string& str2) const
    {
        return (str1 == str2);
    }
};

//----------------------------------------------------------------------------
/**
 * A StringSet is a set with std::string members
 */
class StringSet : public std::set<std::string, StringLessThan> {
public:
    void dump(const char* log) const;
};

//----------------------------------------------------------------------------
/**
 * A StringMap is a map with std::string keys.
 */
template <class _Type> class StringMap :
    public std::map<std::string, _Type, StringLessThan> {
};

//----------------------------------------------------------------------------
/**
 * A StringMultiMap is a multimap with std::string keys.
 */
template <class _Type> class StringMultiMap :
    public std::multimap<std::string, _Type, StringLessThan> {
};

//----------------------------------------------------------------------------
/**
 * A StringHashSet is a hash set with std::string members.
 */
class StringHashSet :
        public _std::unordered_set<std::string, StringHash, StringEquals> {
public:
    void dump(const char* log) const;
};

//----------------------------------------------------------------------------
/**
 * A StringHashMap is a hash map with std::string keys.
 */
template <class _Type> class StringHashMap :
    public _std::unordered_map<std::string, _Type, StringHash, StringEquals> {
};

//----------------------------------------------------------------------------
/**
 * A StringVector is a std::vector of std::strings.
 */
class StringVector : public std::vector<std::string> {
};

//----------------------------------------------------------------------------
/**
 * Tokenize a single string into a vector.
 * Return the number of tokens parsed.
 */
int
tokenize(const std::string& str,
         const std::string& sep,
         std::vector<std::string>* tokens);

//----------------------------------------------------------------------------
/**
 * Generate a hex string from a binary buffer.
 */
inline void
hex2str(std::string* str, const u_char* bp, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    str->erase();
    for (size_t i = 0; i < len; ++i) {
        str->push_back(hex[(bp[i] >> 4) & 0xf]);
        str->push_back(hex[bp[i] & 0xf]);
    }
}

//----------------------------------------------------------------------
/**
 * Ditto that returns a temporary.
 */
inline std::string
hex2str(const u_char* bp, size_t len)
{
    std::string ret;
    hex2str(&ret, bp, len);
    return ret;
}

//----------------------------------------------------------------------
/**
 * A hex2str variant with a char*
 */
inline void
hex2str(std::string* str, const char* bp, size_t len)
{
    return hex2str(str, reinterpret_cast<const u_char*>(bp), len);
}

//----------------------------------------------------------------------
/**
 * A hex2str variant with a char*
 */
inline std::string
hex2str(const char* bp, size_t len)
{
    return hex2str(reinterpret_cast<const u_char*>(bp), len);
}

//----------------------------------------------------------------------------
/**
 * Parse a hex string into a binary buffer. Results undefined if the
 * string contains characters other than [0-9a-f].
 */
inline void
str2hex(const std::string& str, u_char* bp, size_t len)
{
#define HEXTONUM(x) ((x) < 'a' ? (x) - '0' : x - 'a' + 10)
    const char* s = str.data();
    for (size_t i = 0; i < len; ++i) {
        bp[i] = (HEXTONUM(s[2*i]) << 4) + HEXTONUM(s[2*i + 1]);
    }
#undef HEXTONUM
}
 
//----------------------------------------------------------------------------
/**
 * Return true if the string contains only printable characters.
 */
inline bool
str_isascii(const u_char* bp, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (!isascii(*bp++)) {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------
/**
 * Convert an unsigned long to ascii in the given base. The pointer to
 * the tail end of an adequately sized buffer is supplied, and the
 * number of characters written is returned.
 *
 * Implementation largely copied from the FreeBSD 5.0 distribution.
 *
 * @return the number of bytes used or the number that would be used
 * if there isn't enough space in the buffer
 */
inline size_t
fast_ultoa(unsigned long val, int base, char* endp)
{
#define	to_char(n)	((n) + '0')
    char* cp = endp;
    long sval;
    
    static const char xdigs[] = "0123456789abcdef";
    
    switch(base) {
    case 10:
        // optimize one-digit numbers
        if (val < 10) {
            *cp = to_char(val);
            return 1;
        }

        // according to the FreeBSD folks, signed arithmetic may be
        // faster than unsigned, so do at most one unsigned mod/divide
        if (val > LONG_MAX) {
            *cp-- = to_char(val % 10);
            sval = val / 10;
        } else {
            sval = val;
        }
        
        do {
            *cp-- = to_char(sval % 10);
            sval /= 10;
        } while (sval != 0);

        break;
    case 16:
        do {
            *cp-- = xdigs[val & 15];
            val >>= 4;
        } while (val != 0);
        break;

    default:
        return 0; // maybe should do NOTREACHED??
    }

    return endp - cp;

#undef to_char
}

//----------------------------------------------------------------------------
const char*
bool_to_str(bool b);

//----------------------------------------------------------------------------
const char*
str_if(bool b, const char* true_str, const char* false_str = "");

//----------------------------------------------------------------------------
typedef std::pair<std::string, std::string> StringPair;

} // namespace oasys

#endif /* _OASYS_STRING_UTILS_H_ */
