/*
 *    Copyright 2006 SPARTA Inc
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

#ifndef __CIPHERSUITE_PC11_H__
#define __CIPHERSUITE_PC11_H__

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "Ciphersuite_PC.h"

#ifdef BSP_ENABLED

namespace dtn {

class Ciphersuite_PC11 : public Ciphersuite_PC {
    public:
      Ciphersuite_PC11();

      u_int16_t cs_num();

      virtual int encrypt(
      					  std::string       security_dest,
      					  const LocalBuffer&       input,      // the Bundle Encryption Key (BEK) to be encrypted
      					  LocalBuffer&       result);    // encrypted BEK will go here

      virtual int decrypt(
                          std::string   security_src,
                          const LocalBuffer&   input,             // ECDH-encrypted Bundle Encryption Key (BEK)
                          LocalBuffer&   result);		   // decrypted BEK will go here

      const static int CSNUM_PC = 11;

      const static int SEC_LEVEL = 192;

      virtual u_int32_t get_key_len() {
    	  return 256/8;
      };
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_PC11_H_ */
