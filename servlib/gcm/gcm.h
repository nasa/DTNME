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
*/

/*  This file changed 5 June 2007 to reflect name change  
    of included file from "aes.h" to "gcm_aes.h"
    Changed by Peter Lovell, SPARTA Inc., for DTN project.
*/

#ifndef _GCM_H
#define _GCM_H

/*  This define sets the memory alignment that will be used for fast move
    and xor operations on buffers when the alignment matches this value. 
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

#include "gcm_aes.h"
#include "gf128mul.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*  After encryption or decryption operations the return value of
    'compute tag' will be one of the values RETURN_OK, RETURN_WARN
    or RETURN_ERROR, the latter indicating an error. A return value
    RETURN_OK indicates that both encryption and authentication
    have taken place and resulted in the returned tag value. If
    the returned value is RETURN_WARN, the tag value is the result
    of authentication alone without encryption (CCM) or decryption
    (GCM and EAX).
*/
#ifndef RETURN_OK
# define RETURN_WARN      1
# define RETURN_OK        0
# define RETURN_ERROR    -1
#endif

typedef int  ret_type;
dec_unit_type(BFR_UNIT, buf_unit);
dec_bufr_type(BFR_UNIT, AES_BLOCK_SIZE, buf_type);

#define GCM_BLOCK_SIZE  AES_BLOCK_SIZE

/* The GCM-AES  context  */

typedef struct
{
#if defined( TABLES_64K )
    uint_32t        gf_t64k[16][256][GCM_BLOCK_SIZE / 4];
#endif
#if defined( TABLES_8K )
    uint_32t        gf_t8k[32][16][GCM_BLOCK_SIZE / 4];
#endif
#if defined( TABLES_4K )
    uint_32t        gf_t4k[256][GCM_BLOCK_SIZE / 4];
#endif
#if defined( TABLES_256 )
    uint_32t        gf_t256[16][GCM_BLOCK_SIZE / 4];
#endif
    buf_type        ctr_val;                    /* CTR counter value            */
    buf_type        enc_ctr;                    /* encrypted CTR block          */
    buf_type        hdr_ghv;                    /* ghash buffer (header)        */
    buf_type        txt_ghv;                    /* ghash buffer (ciphertext)    */
    buf_type        ghash_h;                    /* ghash H value                */
    aes_encrypt_ctx aes[1];                     /* AES encryption context       */
    uint_32t        y0_val;                     /* initial counter value        */
    uint_32t        hdr_cnt;                    /* header bytes so far          */
    uint_32t        txt_ccnt;                   /* text bytes so far (encrypt)  */
    uint_32t        txt_acnt;                   /* text bytes so far (auth)     */
} gcm_ctx;

/* The following calls handle mode initialisation, keying and completion        */

ret_type gcm_init_and_key(                      /* initialise mode and set key  */
            const unsigned char key[],          /* the key value                */
            unsigned long key_len,              /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_end(                               /* clean up and end operation   */
            gcm_ctx ctx[1]);                    /* the mode context             */

/* The following calls handle complete messages in memory in a single operation */

ret_type gcm_encrypt_message(                   /* encrypt an entire message    */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            unsigned char msg[],                /* the message buffer           */
            unsigned long msg_len,              /* and its length in bytes      */
            unsigned char tag[],                /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

                                    /* RETURN_OK is returned if the input tag   */
                                    /* matches that for the decrypted message   */
ret_type gcm_decrypt_message(                   /* decrypt an entire message    */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            unsigned char msg[],                /* the message buffer           */
            unsigned long msg_len,              /* and its length in bytes      */
            const unsigned char tag[],          /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

/* The following calls handle messages in a sequence of operations followed by  */
/* tag computation after the sequence has been completed. In these calls the    */
/* user is responsible for verfiying the computed tag on decryption             */

ret_type gcm_init_message(                      /* initialise a new message     */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_auth_header(                       /* authenticate the header      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_encrypt(                           /* encrypt & authenticate data  */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_decrypt(                           /* authenticate & decrypt data  */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_compute_tag(                       /* compute authentication tag   */
            unsigned char tag[],                /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

/*  The use of the following calls should be avoided if possible because their
    use requires a very good understanding of the way this encryption mode
    works and the way in which this code implements it in order to use them
    correctly.

    The gcm_auth_data routine is used to authenticate encrypted message data.
    In message encryption gcm_crypt_data must be called before gcm_auth_data
    is called since it is encrypted data that is authenticated.  In message
    decryption authentication must occur before decryption and data can be
    authenticated without being decrypted if necessary.

    If these calls are used it is up to the user to ensure that these routines
    are called in the correct order and that the correct data is passed to them.

    When gcm_compute_tag is called it is assumed that an error in use has
    occurred if both encryption (or decryption) and authentication have taken
    place but the total lengths of the message data respectively authenticated
    and encrypted are not the same. If authentication has taken place but there
    has been no corresponding encryption or decryption operations (none at all)
    only a warning is issued. This should be treated as an error if it occurs
    during encryption but it is only signalled as a warning as it might be
    intentional when decryption operations are involved (this avoids having
    different compute tag functions for encryption and decryption).  Decryption
    operations can be undertaken freely after authetication but if the tag is
    computed after such operations an error will be signalled if the lengths of
    the data authenticated and decrypted don't match.
*/

ret_type gcm_auth_data(                         /* authenticate ciphertext data */
            const unsigned char data[],         /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

ret_type gcm_crypt_data(                        /* encrypt or decrypt data      */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1]);                    /* the mode context             */

#if defined(__cplusplus)
}
#endif

#endif
