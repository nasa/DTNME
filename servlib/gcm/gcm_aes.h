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
 Issue 16/04/2007

*/

/*  This file changed 5 June 2007, extracted from "aes.h", extracting
    only those portions needed to use gcm-mode in an OpenSSL environment.
    Changed by Peter Lovell, SPARTA Inc., for DTN project.
*/

#ifndef _AES_H
#define _AES_H

#include <stdlib.h>
#include <openssl/aes.h>

/*  This include is used to find 8 & 32 bit unsigned integer types  */
#include "brg_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define USE_AES_ENCRYPT /* if support for encryption is needed      */

#define AES_RETURN     VOID_RETURN

/* the encrypt context maps to OpenSSL AES context type             */

#define aes_encrypt_ctx AES_KEY


/* Key lengths in the range 16 <= key_len <= 32 are given in bytes, */
/* those in the range 128 <= key_len <= 256 are given in bits       */

AES_RETURN aes_encrypt_key(const unsigned char *key, int key_len, aes_encrypt_ctx* cx);

#define aes_encrypt AES_encrypt


#if defined(__cplusplus)
}
#endif

#endif
