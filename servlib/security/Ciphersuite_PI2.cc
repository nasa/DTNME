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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#define OPENSSL_FIPS    1       /* required for sha256 */

#include "Ciphersuite_PI2.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "security/EVP_PK.h"

namespace dtn {

int Ciphersuite_PI2::calculate_signature_length( string sec_src, size_t digest_len) {
     size_t sig_len;
     EVP_PK::signature_length(Ciphersuite::config->get_priv_key_sig(sec_src, cs_num()), digest_len, sig_len);

     return sig_len;
}


} // namespace dtn

#endif /* BSP_ENABLED */
