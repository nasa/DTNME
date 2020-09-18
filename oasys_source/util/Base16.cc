#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "Base16.h"

namespace oasys {

//------------------------------------------------------------------------------
size_t 
Base16::encode(const u_int8_t* in, size_t in_len, u_int8_t* out16, size_t out16_len)
{
    const u_int8_t* encoding = reinterpret_cast<const u_int8_t*>("0123456789ABCDEF");

    if (in_len * 2 > out16_len) 
    {
        in_len = out16_len/2;
    }

    size_t i;
    for (i=0; i<in_len; ++i)
    {
        out16[2*i]     = encoding[in[i] & 0xF];
        out16[2*i + 1] = encoding[(in[i] >> 4) & 0xF];
    }
    return i;
}
    
//------------------------------------------------------------------------------
size_t 
Base16::decode(const u_int8_t* in16, size_t in16_len, u_int8_t* out, size_t out_len)
{
    if (in16_len/2 > out_len) 
    {
        in16_len = out_len * 2;
    }

    size_t i;
    for (i=0; i<in16_len; i+=2)
    {
        u_int8_t lower = (in16[i] <= '9') ? (in16[i] - '0') : (in16[i] - 'A' + 10);
        u_int8_t upper = (in16[i+1] <= '9') ? (in16[i+1] - '0') : 
                         (in16[i+1] - 'A' + 10);
        out[i/2] = lower | (upper << 4);
    }

    return i/2;
}

} // namespace tier
