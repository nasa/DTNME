#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <algorithm>
#include "StringAppender.h"

namespace oasys {

//----------------------------------------------------------------------------
StringAppender::StringAppender(char* buf, size_t size)
    : cur_(buf), remaining_(size), len_(0), desired_(0)
{}

//----------------------------------------------------------------------------
size_t 
StringAppender::append(const char* str, size_t len)
{
    if (len == 0)
    {
        len = strlen(str);
    }

    desired_ += len;

    if (remaining_ == 0) 
    {
        return 0;
    }
    
    len = std::min(len, remaining_ - 1);
    memcpy(cur_, str, len);
    cur_[len] = '\0';
    
    cur_       += len;
    remaining_ -= len;
    len_       += len;

    ASSERT(*cur_ == '\0');

    return len;
}

//----------------------------------------------------------------------------
size_t 
StringAppender::append(char c)
{
    ++desired_;

    if (remaining_ <= 1) 
    {
        return 0;
    }
    
    *cur_ = c;
    ++cur_;
    --remaining_;
    ++len_;
    *cur_ = '\0';

    ASSERT(*cur_ == '\0');

    return 1;
}

//----------------------------------------------------------------------------
size_t 
StringAppender::appendf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    size_t ret = vappendf(fmt, ap);
    va_end(ap);

    return ret;
}

//----------------------------------------------------------------------------
size_t 
StringAppender::vappendf(const char* fmt, va_list ap)
{
    size_t ret = log_vsnprintf(cur_, remaining_, fmt, ap);

    desired_ += ret;

    if (remaining_ == 0) 
    {
        return 0;
    }

    ret = std::min(ret, remaining_ - 1);

    cur_       += ret;
    remaining_ -= ret;
    len_       += ret;

    ASSERT(*cur_ == '\0');

    return ret;
}

} // namespace oasys
