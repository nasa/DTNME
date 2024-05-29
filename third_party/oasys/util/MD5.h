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


#ifndef _OASYS_MD5_H_
#define _OASYS_MD5_H_

#include <sys/types.h>
#include <string>

#include "StringUtils.h"
#include "../serialize/Serialize.h"

extern "C" {
#define PROTOTYPES 1
#include "md5-rsa.h"
}

namespace oasys {
/**
 * Simple wrapper class to calculate an MD5 digest.
 */
class MD5 {
public:
    static const unsigned int MD5LEN = 16;
    
    MD5();
    ~MD5() {}
    
    /*! MD5 initialization. Begins an MD5 operation, writing a new context. */
    void init();

    /*! Update the md5 hash with data bytes */ 
    void update(const u_char* data, size_t len);

    /*! Update the md5 hash with data bytes */ 
    void update(const char* data, size_t len);

    /*! Finish up the md5 hashing process */
    void finalize();
    
    /*! \return MD5 hash value. */
    const u_char* digest();

    /*! \return MD5 hash value in ascii, std::string varient */
    static void digest_ascii(std::string* str, const u_char* digest);

    /*! \return MD5 hash value in ascii */
    static std::string digest_ascii(const u_char* digest);

    /*! \return MD5 hash value in ascii */
    void digest_ascii(std::string* str);

    /*! \return MD5 hash value in ascii, std::string varient */
    std::string digest_ascii();

    /*! Obtain the digest from ascii */
    static void digest_fromascii(const char* str, u_char* digest);

private:
    MD5_CTX ctx_;
    u_char digest_[MD5LEN];
};

/**
 * Helper for storing the hash
 */
struct MD5Hash_t : public SerializableObject {
    u_char hash_[MD5::MD5LEN];
    
    MD5Hash_t& operator=(const MD5Hash_t& hash);

    // virtual from SerializableObject
    void serialize(SerializeAction* a);
};

} // namespace oasys

#endif /* _OASYS_MD5_H_ */
