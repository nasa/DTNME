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

#ifndef _KEYSTEWARD_H_
#define _KEYSTEWARD_H_

#ifdef BSP_ENABLED

#include "oasys/util/ScratchBuffer.h"
#include "openssl/cms.h"
#include "openssl/bio.h"

namespace dtn {

typedef oasys::ScratchBuffer<u_char*, 64> DataBuffer;

class Bundle;

class KeyParameterInfo;    //opaque info -- hints for what key to use

class KeySteward  {

public:
    
    static int encrypt(
                       const Bundle*     b,
                       KeyParameterInfo* kpi,
                       const LinkRef&    link,
                       std::string       security_dest,
                       u_char*           data,
                       size_t            data_len,
                       DataBuffer&       db,
                       int csnum);
    
    static int decrypt(
                       const Bundle* b,
                       std::string   security_src,
                       u_char*       enc_data,
                       size_t        enc_data_len,
                       DataBuffer&   db,
                       int csnum);
    
    static int sign(
                    const Bundle*    b,
                    KeyParameterInfo*kpi,
                    const LinkRef&   link,
                    DataBuffer&      db_digest,
                    DataBuffer&      db_signed,
                    int csnum);
    
    static int signature_length(
                                const Bundle*     b,
                                KeyParameterInfo* kpi,
                                const LinkRef&    link,
                                size_t            data_len,
                                size_t&           out_len,
                                int csnum);
    
    static int verify(
                      const Bundle*	b,
                      u_char*       enc_data,
                      size_t        enc_data_len,
                      u_char**      data,
                      size_t*      	data_len,
                      int 			csnum);

};


} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _KEYSTEWARD_H_ */
