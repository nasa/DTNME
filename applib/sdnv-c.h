/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _SDNV_C_H_
#define _SDNV_C_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <sys/types.h>

/**
 * Return the number of bytes needed to encode the given value.
 */
extern size_t sdnv_encoding_len(u_int64_t val);

/**
 * Convert the given 64-bit integer into an SDNV.
 *
 * @return The number of bytes used, or -1 on error.
 */
extern int sdnv_encode(u_int64_t val, u_char* bp, size_t len);

/**
 * Convert an SDNV pointed to by bp into a unsigned 64-bit
 * integer.
 *
 * @return The number of bytes of bp consumed, or -1 on error.
 */
extern int sdnv_decode(const u_char* bp, size_t len, u_int64_t* val);

#ifdef __cplusplus
}
#endif

#endif /* _SDNV_C_H_ */
