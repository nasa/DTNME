/*
 *    Copyright 2006-7 SPARTA Inc
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
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite_ES4.h"
#include "security/EVP_PK.h"

namespace dtn {

//----------------------------------------------------------------------
Ciphersuite_ES4::Ciphersuite_ES4()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_ES4::cs_num(void)
{
    return CSNUM_ES;
}


//----------------------------------------------------------------------
int Ciphersuite_ES4::decrypt(
                             std::string   security_dest,
                             const LocalBuffer&   enc_data,      // RSA-encrypted Bundle Encryption Key (BEK)
                             LocalBuffer&   result)		      // decrypted BEK will go here
{
    return EVP_PK::decrypt(Ciphersuite::config->get_priv_key_dec(security_dest, cs_num()), enc_data, result);
}

int Ciphersuite_ES4::encrypt(
							 std::string       security_dest,
							 const LocalBuffer&       input,          // the Bundle Encryption Key (BEK) to be encrypted
							 LocalBuffer&       result)         // encrypted BEK will go here
{
    return EVP_PK::encrypt(Ciphersuite::config->get_pub_key_enc(security_dest, cs_num()), input, result);
}

} // namespace dtn

#endif /* BSP_ENABLED */
