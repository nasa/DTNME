/*
 *    Copyright 2005-2006 Intel Corporation
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

#include "MD5.h"

namespace oasys {

MD5::MD5() 
{
    init();
}

/*! MD5 initialization. Begins an MD5 operation, writing a new context. */
void 
MD5::init()
{
    MD5Init(&ctx_);
}
    
/*! Update the md5 hash with data bytes */ 
void 
MD5::update(const u_char* data, size_t len)
{
    MD5Update(&ctx_, (u_char*)data, len);
}

/*! Update the md5 hash with data bytes */ 
void 
MD5::update(const char* data, size_t len)
{
    MD5Update(&ctx_, (u_char*)data, len);
}

/*! Finish up the md5 hashing process */
void 
MD5::finalize()
{
    MD5Final(digest_, &ctx_);
}

/*! \return MD5 hash value. */
const u_char* 
MD5::digest()
{
    return digest_;
}

/*! \return MD5 hash value in ascii, std::string varient */
void 
MD5::digest_ascii(std::string* str,
                  const u_char* digest)
{
    hex2str(str, digest, MD5LEN);
}

/*! \return MD5 hash value in ascii */
std::string 
MD5::digest_ascii(const u_char* digest)
{
    std::string str;
    digest_ascii(&str, digest);
    return str;
}

/*! \return MD5 hash value in ascii */
void 
MD5::digest_ascii(std::string* str)
{
    digest_ascii(str, digest_);
}

/*! \return MD5 hash value in ascii, std::string varient */
std::string 
MD5::digest_ascii()
{
    return digest_ascii(digest_);
}

/*! Obtain the digest from ascii */
void 
MD5::digest_fromascii(const char* str, u_char* digest)
{
    str2hex(str, digest, MD5LEN);
}


void 
MD5Hash_t::serialize(SerializeAction* a)
{
    a->process("hash", reinterpret_cast<char*>(hash_), MD5::MD5LEN);
}

MD5Hash_t& 
MD5Hash_t::operator=(const MD5Hash_t& hash)
{
    memcpy(hash_, hash.hash_, MD5::MD5LEN);

    return *this;
}

}; // namespace oasys
