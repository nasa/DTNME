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

#ifndef _OASYS_OPTIONS_H_
#define _OASYS_OPTIONS_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#include <string>
#include <vector>
#include "../compat/inet_aton.h"
#include "../compat/inttypes.h"

#ifdef OASYS_BLUETOOTH_ENABLED
#include <bluetooth/bluetooth.h>
#endif // OASYS_BLUETOOTH_ENABLED

namespace oasys {

class StringBuffer;

/**
 * Base class for options. These can be used either with the Getopt
 * class for parsing argv-style declarations or with the OptParser
 * class for parsing argument strings or arrays of strings.
 */
class Opt {
    friend class Getopt;
    friend class OptParser;
    friend class OptTester;
    friend class TclCommand;

public:
    virtual ~Opt();
    
protected:
    /**
     * Private constructor.
     */
    Opt(char shortopt, const char* longopt,
        void* valp, bool* setp, bool needval,
        const char* valdesc, const char* desc);

    /**
     * Virtual callback to set the option to the given string value.
     */
    virtual int set(const char* val, size_t len) = 0;
    
    /**
     * Virtual callback to get a string version of the current value.
     */
    virtual void get(StringBuffer* buf) = 0;
    
    char        shortopt_;
    const char* longopt_;
    void*       valp_;
    bool*       setp_;
    bool        needval_;
    const char* valdesc_;
    const char* desc_;
};

/**
 * Boolean option class.
 */
class BoolOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt   the option string
     * @param valp  pointer to the value
     * @param desc  descriptive string
     * @param setp  optional pointer to indicate whether or not
                    the option was set
     */
    BoolOpt(const char* opt, bool* valp,
            const char* desc = "", bool* setp = NULL);

    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    BoolOpt(char shortopt, const char* longopt, bool* valp,
            const char* desc = "", bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Integer option class.
 */
class IntOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    IntOpt(const char* opt, int* valp,
           const char* valdesc = "", const char* desc = "",
           bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    IntOpt(char shortopt, const char* longopt, int* valp,
           const char* valdesc = "", const char* desc = "",
           bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Unsigned integer option class.
 */
class UIntOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    UIntOpt(const char* opt, u_int* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    UIntOpt(char shortopt, const char* longopt, u_int* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Unsigned 64-bit option class.
 */
class UInt64Opt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    UInt64Opt(const char* opt, u_int64_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    UInt64Opt(char shortopt, const char* longopt, u_int64_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Unsigned short integer option class.
 */
class UInt16Opt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    UInt16Opt(const char* opt, u_int16_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    UInt16Opt(char shortopt, const char* longopt, u_int16_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Unsigned byte option class.
 */
class UInt8Opt : public Opt {
public:
   /**
    * Basic constructor.
    *
    * @param opt     the option string
    * @param valp    pointer to the value
    * @param valdesc short description for the value
    * @param desc    descriptive string
    * @param setp    optional pointer to indicate whether or not
                     the option was set
    */
    UInt8Opt(const char* opt, u_int8_t* valp,
             const char* valdesc = "", const char* desc = "",
             bool* setp = NULL);

    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc   short description for the value
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    UInt8Opt(char shortopt, const char* longopt, u_int8_t* valp,
             const char* valdesc = "", const char* desc = "",
             bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Option class for sizes that may have units, such as "1B", "10K",
 * "50M", etc.
 */
class SizeOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    SizeOpt(const char* opt, u_int64_t* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    SizeOpt(char shortopt, const char* longopt, u_int64_t* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Option class for bandwidth options, such as "10bps", "10kbps",
 * "50mbps", etc.
 */
class RateOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    RateOpt(const char* opt, u_int64_t* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    RateOpt(char shortopt, const char* longopt, u_int64_t* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Double option class.
 */
class DoubleOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    DoubleOpt(const char* opt, double* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    DoubleOpt(char shortopt, const char* longopt, double* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * String option class.
 */
class StringOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    StringOpt(const char* opt, std::string* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    StringOpt(char shortopt, const char* longopt, std::string* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Char buffer option class.
 */
class CharBufOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param lenp    pointer to the length
     * @param buflen  length of the buffer
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    CharBufOpt(const char* opt, char* valp, size_t* lenp, size_t buflen,
               const char* valdesc = "", const char* desc = "",
               bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param lenp    pointer to the length
     * @param buflen  length of the buffer
     * @param valdesc	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    CharBufOpt(char shortopt, const char* longopt,
               char* valp, size_t* lenp, size_t buflen,
               const char* valdesc = "", const char* desc = "",
               bool* setp = NULL);
    
protected:
    size_t buflen_;
    size_t* lenp_;
    
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Internet address (dotted-quad or DNS name) option class.
 */
class InAddrOpt : public Opt {
public:
    /**
     * Basic constructor.
     *
     * @param opt     the option string
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    InAddrOpt(const char* opt, in_addr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param valp      pointer to the value
     * @param valdesc 	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    InAddrOpt(char shortopt, const char* longopt, in_addr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};

/**
 * Option class to select one of a set of potential values based on
 * string keys.
 */
class EnumOpt : public Opt {
public:
    struct Case {
        const char* key;
        int         val;
    };
    
    /**
     * Basic constructor.
     *
     * @param opt     option string
     * @param cases   pointer to the array of cases
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    EnumOpt(const char* opt, Case* cases, int* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);
    
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param cases     pointer to the array of cases
     * @param valp      pointer to the value
     * @param valdesc 	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    EnumOpt(char shortopt, const char* longopt,
            Case* cases, int* valp,
            const char* valdesc = "", const char* desc = "",
            bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
    Case* cases_;
    bool is_bitflag_;
};

/**
 * BitFlagOpt is a subclass of EnumOpt used for options that are bit
 * flags, hence can be set multiple times.
 */
class BitFlagOpt : public EnumOpt {
public:
    typedef EnumOpt::Case Case;
    
    /**
     * Basic constructor.
     *
     * @param opt     option string
     * @param cases   pointer to the array of cases
     * @param valp    pointer to the value
     * @param valdesc short description for the value 
     * @param desc    descriptive string
     * @param setp    optional pointer to indicate whether or not
                      the option was set
     */
    BitFlagOpt(const char* opt, Case* cases, int* valp,
               const char* valdesc = "", const char* desc = "",
               bool* setp = NULL)
        : EnumOpt(opt, cases, valp, valdesc, desc, setp)
    {
        is_bitflag_ = true;
    }
       
    /**
     * Alternative constructor with both short and long options,
     * suitable for getopt calls.
     *
     * @param shortopt  short option character
     * @param longopt   long option string
     * @param cases     pointer to the array of cases
     * @param valp      pointer to the value
     * @param valdesc 	short description for the value 
     * @param desc      descriptive string
     * @param setp      optional pointer to indicate whether or not
                        the option was set
     */
    BitFlagOpt(char shortopt, const char* longopt,
               Case* cases, int* valp,
               const char* valdesc = "", const char* desc = "",
               bool* setp = NULL)
        : EnumOpt(shortopt, longopt, cases, valp, valdesc, desc, setp)
    {
        is_bitflag_ = true;
    }
};

#ifdef OASYS_BLUETOOTH_ENABLED
/**
 * Bluetooth address (colon-separated hex) option class.
 */
class BdAddrOpt : public Opt {
public:
   /**
    * Basic constructor.
    *
    * @param opt     the option string
    * @param valp    pointer to the value
    * @param valdesc short description for the value
    * @param desc    descriptive string
    * @param setp    optional pointer to indicate whether or not
                     the option was set
    */
    BdAddrOpt(const char* opt, bdaddr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

   /**
    * Alternative constructor with both short and long options,
    * suitable for getopt calls.
    *
    * @param shortopt  short option character
    * @param longopt   long option string
    * @param valp      pointer to the value
    * @param valdesc   short description for the value
    * @param desc      descriptive string
    * @param setp      optional pointer to indicate whether or not 
                       the option was set
    */
    BdAddrOpt(char shortopt, const char* longopt, bdaddr_t* valp,
              const char* valdesc = "", const char* desc = "",
              bool* setp = NULL);

protected:
    int set(const char* val, size_t len);
    void get(StringBuffer* buf);
};
#endif // OASYS_BLUETOOTH_ENABLED

} // namespace oasys

#endif /* _OASYS_OPTIONS_H_ */
