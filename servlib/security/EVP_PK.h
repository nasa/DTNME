/*
 *    Copyright 2007 SPARTA Inc
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

#ifndef _EVP_PK_H_
#define _EVP_PK_H_

#ifdef BSP_ENABLED

#include "oasys/util/ScratchBuffer.h"
#include "openssl/cms.h"
#include "openssl/bio.h"

namespace dtn {

typedef oasys::ScratchBuffer<u_char*, 16> LocalBuffer;

class Bundle;

class EVP_PK  {

public:
    
    static int encrypt(
                       std::string       pub_key_file,
                       const LocalBuffer&       data,
                       LocalBuffer&       result
                       );
    
    static int decrypt(
                       std::string   priv_key_file,
                       const LocalBuffer&       enc_data,
                       LocalBuffer&   db
                       );
    
    static int sign(
                    std::string      priv_key_file,
                    const LocalBuffer&      db_digest,
                    LocalBuffer&      db_signed
                    );
    
    static int signature_length(
                                std::string priv_key_file,
                                size_t            data_len,
                                size_t&           out_len
                                );
    
    static int verify(
                      const LocalBuffer&       enc_data,
                      LocalBuffer&      digest,
                      std::string   pub_key_file
                      );

};


} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _EVP_PK_H_ */
