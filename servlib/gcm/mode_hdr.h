/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2006, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 13/10/2006

 This header file is an INTERNAL file which supports mode implementation
*/

/*  This file changed 5 June 2007 to reflect name change  
    of included file from "aes.h" to "gcm_aes.h"
    Changed by Peter Lovell, SPARTA Inc., for DTN project.
*/

#ifndef _MODE_HDR_H
#define _MODE_HDR_H

/*  This define sets the units in which buffers are processed.  This code
    can provide significant speed gains if buffers can be processed in
    32 or 64 bit chunks rather than in bytes.  This define sets the units
    in which buffers will be accessed if possible
*/
#if !defined( BFR_UNIT )
#  if 1
#    define BFR_UNIT 64
#  elif 0
#    define BFR_UNIT 32
#  else
#    define BFR_UNIT  8
#  endif
#endif

/*  Use of inlines is preferred but code blocks can also be expanded inline
    using 'defines'.  But the latter approach will typically generate a LOT
    of code and is not recommended. 
*/
#if 1 && !defined( USE_INLINING )
#  define USE_INLINING
#endif

#include <string.h>
#include <limits.h>

#if defined( _MSC_VER )
#  if _MSC_VER >= 1400
#    include <stdlib.h>
#    include <intrin.h>
#    pragma intrinsic(memset)
#    pragma intrinsic(memcpy)
#    define rotl32        _rotl
#    define rotr32        _rotr
#    define rotl64        _rotl64
#    define rotr64        _rotl64
#    define bswap_32(x)   _byteswap_ulong(x)
#    define bswap_64(x)   _byteswap_uint64(x)
#  else
#    define rotl32 _lrotl
#    define rotr32 _lrotr
#  endif
#endif

#if BFR_UNIT == 64
#  define NEED_UINT_64T
#endif

#include "brg_endian.h"
#include "brg_types.h"
#include "gcm_aes.h"

#if defined( USE_INLINING )
#  if defined( _MSC_VER )
#    define mh_inline __inline
#  elif defined( __GNUC__ ) || defined( __GNU_LIBRARY__ )
#    define mh_inline static inline
#  else
#    define mh_inline static
#  endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define  ui8_ptr(x)  ptr_cast(x,  8)
#define ui16_ptr(x)  ptr_cast(x, 16)
#define ui32_ptr(x)  ptr_cast(x, 32)
#define ui64_ptr(x)  ptr_cast(x, 64)
#define unit_ptr(x)  ptr_cast(x, BFR_UNIT)

#define BUF_INC     (BFR_UNIT >> 3)
#define BUF_ADRMASK ((BFR_UNIT >> 3) - 1)

/* function pointers might be used for fast XOR operations */

typedef void (*xor_function)(void*, const void* q);

/* left and right rotates on 32 and 64 bit variables */

#if defined( mh_inline )

#if !defined( rotl32 )  // NOTE: 0 <= n <= 32 ASSUMED
mh_inline uint_32t rotl32(uint_32t x, int n)
{
    return (((x) << n) | ((x) >> (32 - n)));
}
#endif

#if !defined( rotr32 )  // NOTE: 0 <= n <= 32 ASSUMED
mh_inline uint_32t rotr32(uint_32t x, int n)
{
    return (((x) >> n) | ((x) << (32 - n)));
}
#endif

#if !defined( rotl64 )  // NOTE: 0 <= n <= 64 ASSUMED
mh_inline uint_64t rotl64(uint_64t x, int n)
{
    return (((x) << n) | ((x) >> (64 - n)));
}
#endif

#if !defined( rotr64 )  // NOTE: 0 <= n <= 64 ASSUMED
mh_inline uint_64t rotr64(uint_64t x, int n)
{
    return (((x) >> n) | ((x) << (64 - n)));
}
#endif

/* byte order inversions for 32 and 64 bit variables */

#if !defined(bswap_32)
mh_inline uint_32t bswap_32(uint_32t x)
{
    return ((rotr32((x), 24) & 0x00ff00ff) | (rotr32((x), 8) & 0xff00ff00));
}
#endif

#if !defined(bswap_64)
mh_inline uint_64t bswap_64(uint_64t x)
{   
    return bswap_32((uint_32t)(x >> 32)) | ((uint_64t)bswap_32((uint_32t)x) << 32);
}
#endif

mh_inline void bswap32_block(void* d, const void* s, int n)
{
    while(n--)
        ((uint_32t*)d)[n] = bswap_32(((uint_32t*)s)[n]);
}

mh_inline void bswap64_block(void* d, const void* s, int n)
{
    while(n--)
        ((uint_64t*)d)[n] = bswap_64(((uint_64t*)s)[n]);
}

#else

#if !defined( rotl32 )  // NOTE: 0 <= n <= 32 ASSUMED
#  define rotl32(x,n)   (((x) << n) | ((x) >> (32 - n)))
#endif

#if !defined( rotr32 )  // NOTE: 0 <= n <= 32 ASSUMED
#  define rotr32(x,n)   (((x) >> n) | ((x) << (32 - n)))
#endif

#if !defined( rotl64 )  // NOTE: 0 <= n <= 64 ASSUMED
#  define rotl64(x,n)   (((x) << n) | ((x) >> (64 - n)))
#endif

#if !defined( rotr64 )  // NOTE: 0 <= n <= 64 ASSUMED
#  define rotr64(x,n)   (((x) >> n) | ((x) << (64 - n)))
#endif

#if !defined(bswap_32)
#  define bswap_32(x) ((rotr32((x), 24) & 0x00ff00ff) | (rotr32((x), 8) & 0xff00ff00))
#endif

#if !defined(bswap_64)
#  define bswap_64(x) (bswap_32((uint_32t)(x >> 32)) | ((uint_64t)bswap_32((uint_32t)x) << 32))
#endif

#define bswap32_block(d,s,n) \
    { int _i = (n); while(_i--) ui32_ptr(d)[_i] = bswap_32(ui32_ptr(s)[_i]); }

#define bswap64_block(d,s,n) \
    { int _i = (n); while(_i--) ui64_ptr(d)[_i] = bswap_64(ui64_ptr(s)[_i]); }

#endif

/* support for fast aligned buffer move and XOR operations */

#if defined( mh_inline )

mh_inline void move_block(void* p, const void* q)
{
    memcpy(p, q, 16);
}

mh_inline void move_block_aligned( void *p, const void *q)
{
#if BFR_UNIT == 8
    move_block(p, q);
#else
    unit_ptr(p)[0] = unit_ptr(q)[0]; unit_ptr(p)[1] = unit_ptr(q)[1];
#  if BFR_UNIT == 32
    unit_ptr(p)[2] = unit_ptr(q)[2]; unit_ptr(p)[3] = unit_ptr(q)[3];
#  endif
#endif
}

mh_inline void xor_block(void* p, const void* q)
{
    ui8_ptr(p)[ 0] ^= ui8_ptr(q)[ 0]; ui8_ptr(p)[ 1] ^= ui8_ptr(q)[ 1];
    ui8_ptr(p)[ 2] ^= ui8_ptr(q)[ 2]; ui8_ptr(p)[ 3] ^= ui8_ptr(q)[ 3];
    ui8_ptr(p)[ 4] ^= ui8_ptr(q)[ 4]; ui8_ptr(p)[ 5] ^= ui8_ptr(q)[ 5];
    ui8_ptr(p)[ 6] ^= ui8_ptr(q)[ 6]; ui8_ptr(p)[ 7] ^= ui8_ptr(q)[ 7];
    ui8_ptr(p)[ 8] ^= ui8_ptr(q)[ 8]; ui8_ptr(p)[ 9] ^= ui8_ptr(q)[ 9];
    ui8_ptr(p)[10] ^= ui8_ptr(q)[10]; ui8_ptr(p)[11] ^= ui8_ptr(q)[11];
    ui8_ptr(p)[12] ^= ui8_ptr(q)[12]; ui8_ptr(p)[13] ^= ui8_ptr(q)[13];
    ui8_ptr(p)[14] ^= ui8_ptr(q)[14]; ui8_ptr(p)[15] ^= ui8_ptr(q)[15];
}

mh_inline void xor_block_aligned( void *p, const void *q)
{
#if BFR_UNIT == 8
    xor_block(p, q);
#else
    unit_ptr(p)[0] ^= unit_ptr(q)[0]; unit_ptr(p)[1] ^= unit_ptr(q)[1];
#  if BFR_UNIT == 32
    unit_ptr(p)[2] ^= unit_ptr(q)[2]; unit_ptr(p)[3] ^= unit_ptr(q)[3];
#  endif
#endif
}

#else

#define move_block(p,q) memcpy((p), (q), 16)

#if BFR_UNIT == 64
#  define move_block_aligned(p,q) \
   ui64_ptr(p)[0] = ui64_ptr(q)[0], ui64_ptr(p)[1] = ui64_ptr(q)[1]
#elif BFR_UNIT == 32
#  define move_block_aligned(p,q) \
   ui32_ptr(p)[0] = ui32_ptr(q)[0], ui32_ptr(p)[1] = ui32_ptr(q)[1], \
   ui32_ptr(p)[2] = ui32_ptr(q)[2], ui32_ptr(p)[3] = ui32_ptr(q)[3]
#else
#  define move_block_aligned(p,q) move_block(p,q)
#endif

#define xor_block(p,q) \
    ui8_ptr(p)[ 0] ^= ui8_ptr(q)[ 0], ui8_ptr(p)[ 1] ^= ui8_ptr(q)[ 1], \
    ui8_ptr(p)[ 2] ^= ui8_ptr(q)[ 2], ui8_ptr(p)[ 3] ^= ui8_ptr(q)[ 3], \
    ui8_ptr(p)[ 4] ^= ui8_ptr(q)[ 4], ui8_ptr(p)[ 5] ^= ui8_ptr(q)[ 5], \
    ui8_ptr(p)[ 6] ^= ui8_ptr(q)[ 6], ui8_ptr(p)[ 7] ^= ui8_ptr(q)[ 7], \
    ui8_ptr(p)[ 8] ^= ui8_ptr(q)[ 8], ui8_ptr(p)[ 9] ^= ui8_ptr(q)[ 9], \
    ui8_ptr(p)[10] ^= ui8_ptr(q)[10], ui8_ptr(p)[11] ^= ui8_ptr(q)[11], \
    ui8_ptr(p)[12] ^= ui8_ptr(q)[12], ui8_ptr(p)[13] ^= ui8_ptr(q)[13], \
    ui8_ptr(p)[14] ^= ui8_ptr(q)[14], ui8_ptr(p)[15] ^= ui8_ptr(q)[15]

#if BFR_UNIT == 64
#  define xor_block_aligned(p,q) \
   ui64_ptr(p)[0] ^= ui64_ptr(q)[0], ui64_ptr(p)[1] ^= ui64_ptr(q)[1]
#elif BFR_UNIT == 32
#  define xor_block_aligned(p,q) \
   ui32_ptr(p)[0] ^= ui32_ptr(q)[0], ui32_ptr(p)[1] ^= ui32_ptr(q)[1], \
   ui32_ptr(p)[2] ^= ui32_ptr(q)[2], ui32_ptr(p)[3] ^= ui32_ptr(q)[3]
#else
#  define xor_block_aligned(p,q)  xor_block(p,q)
#endif

#endif

/* platform byte order to big or little endian order for 32 and 64 bit variables */

#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN
#  define uint_32t_to_le(x) (x) = bswap_32((x))
#  define uint_64t_to_le(x) (x) = bswap_64((x))
#  define uint_32t_to_be(x)
#  define uint_64t_to_be(x)
#else
#  define uint_32t_to_le(x)
#  define uint_64t_to_le(x)
#  define uint_32t_to_be(x) (x) = bswap_32((x))
#  define uint_64t_to_be(x) (x) = bswap_64((x))
#endif

#if defined(__cplusplus)
}
#endif

#endif
