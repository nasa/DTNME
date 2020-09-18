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

#ifndef _CIPHERSUITE_ES8_H_
#define _CIPHERSUITE_ES8_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "Ciphersuite_ES.h"

#ifdef BSP_ENABLED

namespace dtn {

class Ciphersuite_ES8 : public Ciphersuite_ES {
    public:
      Ciphersuite_ES8();

      u_int16_t cs_num();

      virtual int encrypt(
      					  std::string       security_dest,
      					  const LocalBuffer&       input,     // the Bundle Encryption Key (BEK) to be encrypted
      					  LocalBuffer&       result);   // encrypted BEK will go here

      virtual int decrypt(
                          std::string   security_dest,
                          const LocalBuffer&   enc_data,      // ECDH-encrypted Bundle Encryption Key (BEK)
                          LocalBuffer&   result);	   // decrypted BEK will go here

      const static int CSNUM_ES = 8;

 	  virtual u_int32_t get_key_len() {
 		  return 128/8;
 	  };

};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_ES8_H_ */
