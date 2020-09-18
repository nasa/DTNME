#ifndef __ECDH__
#define __ECDH__

#ifdef BSP_ENABLED

#include "openssl/cms.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/asn1.h"
#include "openssl/asn1t.h"
#include "openssl/rand.h"
#include "openssl/aes.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"

#include "oasys/util/ScratchBuffer.h"

typedef oasys::ScratchBuffer<u_char*, 16> LocalBuffer;

typedef struct OUR_ContentInfo_st OUR_ContentInfo;

namespace dtn {

class ECDH {
  public:

	/**
	 * Encrypt the given data (i.e. the BEK) using AES-CBC to the recipient identified in the certificate
	 * (i.e. choose a public elliptic curve key, compute the ECDH shared secret, and encrypt the
	 * data with a symmetric key derived from the shared secret). The cms structure will be
	 * filled in with the public (ephemeral) EC key, and also the AES key wrap.
	 */
    static int encrypt(const LocalBuffer&	data_in,
    				   const char*			cert_filename,
                       LocalBuffer &cms_out,
    				   int 					sec_lev=128);

	/**
	 * Decrypt the given data (i.e. the BEK) using AES-CBC with the given private key, as well as the public
	 * (ephemeral) elliptic curve public key and AES key wrap in the CMS structure.
	 */
    static int decrypt(LocalBuffer &out,
    			       const char*			priv_filename,
    			       const LocalBuffer &cms_int);

 protected:

	/**
	 * Pad the input data to a multiple of 16 bytes
	 */
    static int pad(unsigned char**	out, 
		   const unsigned char*	in, 
		   const int 		inlen);

	/**
	 * Return the number of padding bytes in the input data
	 */
    static int unpad(const unsigned char*	in, 
		     const int 			inlen);

	/**
	 *
	 */
    static void print_buffer(const char*		label, 
			     const unsigned char* 	buf, 
			     const int 			len);

	/**
	 * Hashes the User Keying Material (UKM) and shared data to create an 
         * symmetric encryption key of length sec_lev
	 */
    static unsigned char *derive_key(const unsigned char*	ukm, 
				     const unsigned char*	shared_data, 
				     int 			sec_lev=128);

	/**
	 * Read in the certificate from a file
	 */
    static  X509 *get_cert(const char *filename);

	/**
	 * Build the CMS structure to hold all the keying material
	 */
    static void build_cms(OUR_ContentInfo**	cms, 
			 X509*			rcert, 
			 EC_KEY*		ephemeral, 
			 const EC_GROUP*	group, 
			 unsigned char*		iv, 
			 unsigned char*		ukm, 
			 unsigned char*		wrapped_key, 
			 unsigned char* 	content, 
			 size_t 		content_len, 
			 int sec_lev=128);
};

} // namespace dtn
         
#endif /* BSP_ENABLED */

#endif /* __ECDH__ */
