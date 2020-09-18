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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#ifdef OASYS_BLUETOOTH_ENABLED
#include "bluez/Bluetooth.h"
#endif // OASYS_BLUETOOTH_ENABLED

#include <stdio.h>
#include <unistd.h>

#include "Options.h"
#include "io/NetUtils.h"
#include "util/StringBuffer.h"

namespace oasys {

//----------------------------------------------------------------------
Opt::Opt(char shortopt, const char* longopt,
         void* valp, bool* setp, bool needval,
         const char* valdesc, const char* desc)
    
    : shortopt_(shortopt),
      longopt_(longopt),
      valp_(valp),
      setp_(setp),
      needval_(needval),
      valdesc_(valdesc),
      desc_(desc)
{
    if (setp) *setp = false;
}

//----------------------------------------------------------------------
Opt::~Opt()
{
}

//----------------------------------------------------------------------
BoolOpt::BoolOpt(const char* opt, bool* valp,
                 const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, false, "", desc)
{
}

//----------------------------------------------------------------------
BoolOpt::BoolOpt(char shortopt, const char* longopt, bool* valp,
                 const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, false, "", desc)
{
}

//----------------------------------------------------------------------
int
BoolOpt::set(const char* val, size_t len)
{
    if ((val == 0) ||
        (strncasecmp(val, "t", len) == 0)     ||
        (strncasecmp(val, "true", len) == 0) ||
        (strncasecmp(val, "1", len) == 0))
    {
        *((bool*)valp_) = true;
    }
    else if ((strncasecmp(val, "f", len) == 0)     ||
             (strncasecmp(val, "false", len) == 0) ||
             (strncasecmp(val, "0", len) == 0))
    {
        *((bool*)valp_) = false;
    }
    else
    {
        return -1;
    }

    if (setp_)
        *setp_ = true;

    return 0;
}

//----------------------------------------------------------------------
void
BoolOpt::get(StringBuffer* buf)
{
    if (*((bool*)valp_)) {
        buf->appendf("true");
    } else {
        buf->appendf("false");
    }
}

//----------------------------------------------------------------------
IntOpt::IntOpt(const char* opt, int* valp,
               const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
IntOpt::IntOpt(char shortopt, const char* longopt, int* valp,
               const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
IntOpt::set(const char* val, size_t len)
{
    int newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtol(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;
            
    *((int*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
IntOpt::get(StringBuffer* buf)
{
    buf->appendf("%d", *(int*)valp_);
}

//----------------------------------------------------------------------
UIntOpt::UIntOpt(const char* opt, u_int* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
UIntOpt::UIntOpt(char shortopt, const char* longopt, u_int* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
UIntOpt::set(const char* val, size_t len)
{
    u_int newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtoul(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;
            
    *((u_int*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
UIntOpt::get(StringBuffer* buf)
{
    buf->appendf("%u", *(u_int*)valp_);
}

//----------------------------------------------------------------------
UInt64Opt::UInt64Opt(const char* opt, u_int64_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
UInt64Opt::UInt64Opt(char shortopt, const char* longopt, u_int64_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
UInt64Opt::set(const char* val, size_t len)
{
    u_int64_t newval;
    char* endptr = 0;
    
    if (len == 0)
        return -1;
    
    newval = strtoull(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;
            
    *((u_int64_t*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
UInt64Opt::get(StringBuffer* buf)
{
    buf->appendf("%llu", U64FMT((*(u_int64_t*)valp_)));
}

//----------------------------------------------------------------------
UInt16Opt::UInt16Opt(const char* opt, u_int16_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
UInt16Opt::UInt16Opt(char shortopt, const char* longopt, u_int16_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
UInt16Opt::set(const char* val, size_t len)
{
    u_int newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtoul(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;

    if (newval > 65535)
        return -1;
            
    *((u_int16_t*)valp_) = (u_int16_t)newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
UInt16Opt::get(StringBuffer* buf)
{
    buf->appendf("%u", *(u_int16_t*)valp_);
}

//----------------------------------------------------------------------
UInt8Opt::UInt8Opt(const char* opt, u_int8_t* valp,
                   const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
UInt8Opt::UInt8Opt(char shortopt, const char* longopt, u_int8_t* valp,
                   const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
UInt8Opt::set(const char* val, size_t len)
{
    u_int newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtoul(val, &endptr, 0);
    if (endptr != (val + len))
        return -1;

    if (newval > 255)
        return -1;

    *((u_int8_t*)valp_) = (u_int8_t)newval;

    if (setp_)
        *setp_ = true;

    return 0;
}

//----------------------------------------------------------------------
void
UInt8Opt::get(StringBuffer* buf)
{
    buf->appendf("%u", *(u_int8_t*)valp_);
}

//----------------------------------------------------------------------
SizeOpt::SizeOpt(const char* opt, u_int64_t* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
SizeOpt::SizeOpt(char shortopt, const char* longopt, u_int64_t* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
SizeOpt::set(const char* val, size_t len)
{
    u_int64_t newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtoull(val, &endptr, 0);

    if (endptr == val)
        return -1;

    if (endptr != (val + len))
    {
        if ((endptr + 1) != (val + len))
            return -1;

        switch (*endptr) {
        case 'B': case 'b': break;
        case 'K': case 'k': newval *= 1024; break;
        case 'M': case 'm': newval *= 1048576; break;
        case 'G': case 'g': newval *= 1073741824; break;
        default:
            return -1;
        }
    }
            
    *((u_int64_t*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
SizeOpt::get(StringBuffer* buf)
{
    buf->appendf("%llu", U64FMT((*(u_int64_t*)valp_)));
}

//----------------------------------------------------------------------
RateOpt::RateOpt(const char* opt, u_int64_t* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
RateOpt::RateOpt(char shortopt, const char* longopt, u_int64_t* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
RateOpt::set(const char* val, size_t len)
{
    u_int64_t newval;
    char* endptr = 0;

    newval = strtoull(val, &endptr, 0);

    if (endptr == val || len == 0)
        return -1;

    if (endptr != (val + len))
    {
        size_t unitlen = len - (endptr - val);
        if (!strncasecmp(endptr, "bps", unitlen)) {
            // nothing to change
        } else if (!strncasecmp(endptr, "kbps", unitlen)) {
            newval *= 1000;
        } else if (!strncasecmp(endptr, "mbps", unitlen)) {
            newval *= 1000000;
        } else if (!strncasecmp(endptr, "gbps", unitlen)) {
            newval *= 1000000000LL;
        } else {
            return -1;
        }
    }
    
    *((u_int64_t*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
RateOpt::get(StringBuffer* buf)
{
    buf->appendf("%llu", U64FMT((*(u_int64_t*)valp_)));
}

//----------------------------------------------------------------------
DoubleOpt::DoubleOpt(const char* opt, double* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
DoubleOpt::DoubleOpt(char shortopt, const char* longopt, double* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
DoubleOpt::set(const char* val, size_t len)
{
    double newval;
    char* endptr = 0;

    if (len == 0)
        return -1;
    
    newval = strtod(val, &endptr);
    if (endptr != (val + len))
        return -1;
            
    *((double*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
DoubleOpt::get(StringBuffer* buf)
{
    buf->appendf("%f", *(double*)valp_);
}

//----------------------------------------------------------------------
StringOpt::StringOpt(const char* opt, std::string* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
StringOpt::StringOpt(char shortopt, const char* longopt, std::string* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
StringOpt::set(const char* val, size_t len)
{
    ((std::string*)valp_)->assign(val, len);

    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
StringOpt::get(StringBuffer* buf)
{
    buf->appendf("%s", ((std::string*)valp_)->c_str());
}

//----------------------------------------------------------------------
CharBufOpt::CharBufOpt(const char* opt, char* valp, size_t* lenp, size_t buflen,
                       const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc), buflen_(buflen), lenp_(lenp)
{
}

//----------------------------------------------------------------------
CharBufOpt::CharBufOpt(char shortopt, const char* longopt,
                       char* valp, size_t* lenp, size_t buflen,
                       const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc),
      buflen_(buflen), lenp_(lenp)
{
}

//----------------------------------------------------------------------
int
CharBufOpt::set(const char* val, size_t len)
{
    if (len > buflen_) {
        return -1;
    }

    memcpy(valp_, val, len);

    *lenp_ = len;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
CharBufOpt::get(StringBuffer* buf)
{
    buf->appendf("%.*s", (int)*lenp_, (char*)valp_);
}

//----------------------------------------------------------------------
InAddrOpt::InAddrOpt(const char* opt, in_addr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
InAddrOpt::InAddrOpt(char shortopt, const char* longopt, in_addr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
InAddrOpt::set(const char* val, size_t len)
{
    (void)len;
    
    in_addr_t newval;

    if (oasys::gethostbyname(val, &newval) != 0) {
        return -1;
    }
        
    *((in_addr_t*)valp_) = newval;
    
    if (setp_)
        *setp_ = true;
    
    return 0;
}

//----------------------------------------------------------------------
void
InAddrOpt::get(StringBuffer* buf)
{
    buf->appendf("%s", intoa(*((in_addr_t*)valp_)));
}

//----------------------------------------------------------------------
EnumOpt::EnumOpt(const char* opt, Case* cases, int* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc),
      cases_(cases),
      is_bitflag_(false)
{
}

//----------------------------------------------------------------------
EnumOpt::EnumOpt(char shortopt, const char* longopt,
                 Case* cases, int* valp,
                 const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc), 
      cases_(cases),
      is_bitflag_(false)
{
}

//----------------------------------------------------------------------
int
EnumOpt::set(const char* val, size_t len)
{
    (void)len;
    
    size_t i = 0;

    while (cases_[i].key != 0)
    {
        if (!strcasecmp(cases_[i].key, val)) {

            if (is_bitflag_) {
                (*(int*)valp_) |= cases_[i].val;
            } else {
                (*(int*)valp_) = cases_[i].val;
            }
            
            if (setp_)
                *setp_ = true;
            
            return 0;
        }
        ++i;
    }
    
    return -1;
}

//----------------------------------------------------------------------
void
EnumOpt::get(StringBuffer* buf)
{
    size_t i = 0;

    while (cases_[i].key != 0)
    {
        int val = (*(int*)valp_);

        if (val == cases_[i].val ||
            (is_bitflag_ && (val & cases_[i].val)))
        {
            buf->append(cases_[i].key);
            if (! is_bitflag_)
                return;
        }
        ++i;
    }
}

#ifdef OASYS_BLUETOOTH_ENABLED
//----------------------------------------------------------------------
BdAddrOpt::BdAddrOpt(const char* opt, bdaddr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
BdAddrOpt::BdAddrOpt(char shortopt, const char* longopt, bdaddr_t* valp,
                     const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

//----------------------------------------------------------------------
int
BdAddrOpt::set(const char* val, size_t len)
{
    bdaddr_t newval;
    (void)len;

    /* returns NULL on failure */
    if (Bluetooth::strtoba(val, &newval) == 0) {
        return -1;
    }

    *((bdaddr_t*)valp_) = newval;

    if (setp_)
        *setp_ = true;

    return 0;
}

//----------------------------------------------------------------------
void
BdAddrOpt::get(StringBuffer* buf)
{
    buf->appendf("%s", bd2str(*((bdaddr_t*)valp_)));
}

#endif // OASYS_BLUETOOTH_ENABLED

} // namespace oasys
