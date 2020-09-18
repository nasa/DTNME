#ifndef __BASE16_H__
#define __BASE16_H__

#include "../debug/DebugUtils.h"
#include "../compat/inttypes.h"

namespace oasys {

class Base16 {
public:
    /*!
     * Encode input into Base16. out should be *2 the size of in. Will
     * truncate buffer if there is not enough space.
     *
     * @return Number of bytes encoded.
     */
    static size_t encode(const u_int8_t* in, size_t in_len, 
                         u_int8_t* out16, size_t out16_len);
    
    /*!
     * Decode the input from Base16. out should be 1/2 the size of
     * in. Will truncate buffer if there is not enough space.
     *
     * @return Number of bytes decoded.
     */
    static size_t decode(const u_int8_t* in16, size_t in16_len, 
                         u_int8_t* out, size_t out_len);
};

};

#endif /* __BASE16_H__ */
