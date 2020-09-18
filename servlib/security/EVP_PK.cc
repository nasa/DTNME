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

#include <limits.h>
#include "Ciphersuite.h"
#include "bundling/Bundle.h"
#include "contacts/Link.h"
#include "security/EVP_PK.h"


#include "openssl/cms.h"
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include <openssl/err.h>
#include <bundling/SDNV.h>
#include <exception>
#include <string>
#include <dirent.h>
#include <sys/types.h>


#define FAIL_IF(test, ...) if(test) { log_err_p(log, __VA_ARGS__); goto err; }
#define FAIL_IF_NULL(test, ...) if(test==NULL) { log_err_p(log, __VA_ARGS__); goto err; }

typedef struct CMS_SignerIdentifier_st CMS_SignerIdentifier;


struct CMS_SignerInfo_st
{
	long version;
	CMS_SignerIdentifier *sid;
	X509_ALGOR *digestAlgorithm;
	STACK_OF(X509_ATTRIBUTE) *signedAttrs;
	X509_ALGOR *signatureAlgorithm;
	ASN1_OCTET_STRING *signature;
	STACK_OF(X509_ATTRIBUTE) *unsignedAttrs;
	X509 *signer;
	EVP_PKEY *pkey;
};


static int cms_add1_signingTime(CMS_SignerInfo *si, ASN1_TIME *t)
{
	ASN1_TIME *tt;
	int r = 0;
	if (t)
		tt = t;
	else
		tt = X509_gmtime_adj(NULL, 0);

	if (!tt)
		goto err;

	if (CMS_signed_add1_attr_by_NID(si, NID_pkcs9_signingTime,
						tt->type, tt, -1) <= 0)
		goto err;

	r = 1;

	err:
	if (!t)
		ASN1_TIME_free(tt);

	if (!r)
		CMSerr(CMS_F_CMS_ADD1_SIGNINGTIME, ERR_R_MALLOC_FAILURE);

	return r;
}



namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
void
openssl_errors()
{
    ERR_load_crypto_strings();

    BIO *temp;
    temp = BIO_new(BIO_s_mem());

	ERR_print_errors(temp);
    int len = BIO_ctrl_pending(temp);

    if(len > 0) {
    	char *output = (char *)malloc(len+1);
        output[len] = '\0';
    	BIO_read(temp, (char*)output, len);
    	log_err_p(log, "openssl error: %s", output);
    	free(output);
    }
    BIO_free(temp);
}


//----------------------------------------------------------------------
int
EVP_PK::encrypt(std::string       certfile,
                    const LocalBuffer&       input,         	// the BEK to be encrypted
                    LocalBuffer&       encrypted_data   // encrypted BEK will go here
                    )       
{
    u_char*   			buf;
    size_t    			size;
    X509*				cert = NULL;      // X509 certificate for the destination node
    STACK_OF(X509)*		certchain = NULL; // chain of certificates
    CMS_ContentInfo*	cms = NULL;       // CMS structure
    BIO*				data_bio = NULL;
    BIO*				out_bio = NULL;
    BIO*				cert_bio = NULL;
    int 				flags = CMS_STREAM | CMS_BINARY | CMS_NOCERTS;
    int 				ret = -1;
    u_char *data = input.buf();
    size_t data_len = input.len();

    // openssl initialization
    OpenSSL_add_all_algorithms();

    if ( data_len > USHRT_MAX )
        return -1;
    
    if ( data_len < 8 )     //sanity check for minimum key size
        return -1;
  
    log_debug_p(log,"EVP_PK::encrypt: input data_len  = %zu .", data_len);
    log_debug_p(log,"EVP_PK::encrypt: input data (= BEK) (1st 16 bytes) :  %s", Ciphersuite::buf2str(data, 16).c_str());
    OpenSSL_add_all_algorithms();

    cert_bio = BIO_new_file(certfile.c_str(), "r");
    FAIL_IF(cert_bio == NULL, "EVP_PK::encrypt: Error opening cert %s .", certfile.c_str());

    // process X509 cert
    cert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
    FAIL_IF(cert == NULL, "EVP_PK::encrypt: Error processing cert %s .", certfile.c_str());

    // CMS_encrypt() expects a stack of certificates
    certchain = sk_X509_new_null();
    FAIL_IF(!certchain || !sk_X509_push(certchain, cert), "EVP_PK::encrypt: Error STACKing certs.");
    // set cert to NULL so it isn't freed up twice.
    cert = NULL;

    // read input data (i.e. BEK) into a BIO
    data_bio = BIO_new_mem_buf(data, data_len);
    FAIL_IF(!data_bio, "EVP_PK::encrypt: Error creating memory BIO for input data.");

    // encrypt the data (i.e. BEK) and put into CMS_EnvelopedData structure
    cms = CMS_encrypt(certchain, data_bio, EVP_des_ede3_cbc(), flags);
    FAIL_IF(!cms, "EVP_PK::encrypt: CMS_encrypt: failed.");
    log_debug_p(log, "EVP_PK::encrypt: CMS_encrypt: successful");

    out_bio = BIO_new(BIO_s_mem());
    FAIL_IF(!out_bio, "EVP_PK::encrypt: Error creating memory BIO for output.");

    // write CMS_EnvelopedData structure out in BER format
    FAIL_IF(!i2d_CMS_bio_stream(out_bio, cms, data_bio, flags), "EVP_PK::encrypt: Error writing CMS structure Failed");

    // Determine length of CMS_EnvelopedData structure
    size = BIO_ctrl_pending(out_bio);
    log_debug_p(log, "EVP_PK::encrypt: CMS_EnvelopedData length: %zu bytes",size);

    // Allocate memory and copy BER encoded CMS_EnvelopedData structure into buffer
    encrypted_data.reserve(size);
    encrypted_data.set_len(size);
    buf = encrypted_data.buf();

    FAIL_IF(BIO_read(out_bio, (u_char*)buf, size) <= 0, "EVP_PK::encrypt: BIO_read failed.");

    log_debug_p(log, "EVP_PK::encrypt: CMS_EnvelopedData (1st 32 bytes) :  %s", Ciphersuite::buf2str(buf, 32).c_str());
    log_debug_p(log, "EVP_PK::encrypt: CMS_EnvelopedData (LAST 32 bytes) :  %s", Ciphersuite::buf2str(buf+size-32, 32).c_str());

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::encrypt: Failed");
    	openssl_errors();
    }

    // clean up
    if (cms)
    	CMS_ContentInfo_free(cms);
    if (cert)
    	X509_free(cert);
    if (certchain)
    	sk_X509_pop_free(certchain, X509_free);
    if (data_bio)
    	BIO_free(data_bio);
    if (cert_bio)
    	BIO_free(cert_bio);
    if (out_bio)
    	BIO_free(out_bio);

    return ret;
}




//----------------------------------------------------------------------
int
EVP_PK::decrypt(std::string  priv_key_file,
                    const LocalBuffer&   enc_data_input,  // BER-encoded CMS_EnvelopedData holding BEK
                    LocalBuffer&   data_buf		   // this will contain the decrypted BEK
                    )
{
    
    size_t  			data_len;
    u_char*				buf;
    CMS_ContentInfo* 	cms = NULL;
    BIO*				out_bio = NULL;
    BIO*				enc_data_bio = NULL;
    BIO*				key_bio = NULL;
    int 				ret = -1;
    X509*				cert = NULL;			// our own certificate is input to the decryption function
    EVP_PKEY*			evp_priv_key = NULL;	// our private decryption key
    u_char * enc_data = enc_data_input.buf();
    size_t enc_data_len = enc_data_input.len();


    // openssl initialization
    OpenSSL_add_all_algorithms();

    if (enc_data_len < 2)
        return -1;
 
    log_debug_p(log, "EVP_PK::decrypt: enc_data_len: %zu bytes", enc_data_len);
    log_debug_p(log, "EVP_PK::decrypt: enc_data (= CMS_EnvelopedData) (1st 32 bytes):  %s", Ciphersuite::buf2str(enc_data, 32).c_str());

    // read in the encrypted data
    enc_data_bio = BIO_new_mem_buf(enc_data, enc_data_len);
    if (!enc_data_bio) {
	    log_err_p(log, "EVP_PK::decrypt: Error creating BIO from encrypted data.");
	    goto err;
    }

    // extract CMS_EnvelopedData structure
    cms = d2i_CMS_bio(enc_data_bio, NULL);
    FAIL_IF(!cms, "EVP_PK::decrypt: Error reading in CMS structure.");

    out_bio = BIO_new(BIO_s_mem());
    FAIL_IF(!out_bio, "EVP_PK::decrypt: Error creating memory BIO for decrypted output.");
  
    key_bio = BIO_new_file(priv_key_file.c_str(), "r"); //open private key file in the form of a BIO
    FAIL_IF(key_bio == NULL, "EVP_PK::decrypt: Error opening %s .", priv_key_file.c_str());

    cert = PEM_read_bio_X509(key_bio, NULL, 0, NULL); // process cert using X509 structure
    FAIL_IF(cert == NULL, "EVP_PK::decrypt: Error processing cert from %s .", priv_key_file.c_str());

    BIO_reset(key_bio);  // move file pointer to start of file

    evp_priv_key = PEM_read_bio_PrivateKey(key_bio, NULL, 0, NULL);  // read in key
    FAIL_IF(evp_priv_key == NULL, "EVP_PK::decrypt: Error reading private key from  %s .", priv_key_file.c_str());

    // decrypt the content in the CMS_EnvelopedData structure
    FAIL_IF(!CMS_decrypt(cms, evp_priv_key, cert, NULL, out_bio, 0), "EVP_PK::decrypt: CMS_decrypt: failed.");

    // Determine length of the decrypted content
    data_len = BIO_ctrl_pending(out_bio);
    log_debug_p(log, "EVP_PK::decrypt: decrypted content len: %zu bytes", data_len);

    // Allocate memory and copy decrypted content
    data_buf.reserve(data_len);
    data_buf.set_len(data_len);
    buf = data_buf.buf();
    FAIL_IF(BIO_read(out_bio, (u_char*)buf, data_len) <= 0, "EVP_PK::decrypt: BIO_read failed.");
   
    log_debug_p(log, "EVP_PK::decrypt: decrypted content (i.e. BEK): %s", Ciphersuite::buf2str(data_buf).c_str());

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::decrypt: Failed");
    	openssl_errors();
    }

    // clean up
    if (cert)
    	X509_free(cert);
    if (key_bio)
    	BIO_free(key_bio);
    if (evp_priv_key)
    	EVP_PKEY_free(evp_priv_key);
    if (cms)
    	CMS_ContentInfo_free(cms);
    if (enc_data_bio)
    	BIO_free(enc_data_bio);
    if (out_bio)
    	BIO_free(out_bio);

    return ret;
}

//----------------------------------------------------------------------
int
create_signer_info(std::string priv_key_file,
                    CMS_ContentInfo*	cms,
					CMS_SignerInfo**	signer_info,
					EVP_PKEY**			evp_priv_key,
					const LocalBuffer&			db_digest
	                )
{
	size_t    	    	digest_len = db_digest.len();
	u_char*   	    	digest = db_digest.buf();
	X509*           	cert = NULL;
	BIO*				key_bio = NULL;
	int 				flags = CMS_NOCERTS | CMS_DETACHED | CMS_NOATTR;
	int 				ret = -1;

	FAIL_IF(digest_len > USHRT_MAX, "EVP_PK::create_signer_info: digest length is too long to represent correctly here");

    // open private key file in the form of a BIO
    key_bio = BIO_new_file(priv_key_file.c_str(), "r");
    FAIL_IF(key_bio == NULL, "EVP_PK::create_signer_info: Error opening %s .", priv_key_file.c_str());

    // process X509 certificate
    cert = PEM_read_bio_X509(key_bio, NULL, 0, NULL);
    FAIL_IF(cert == NULL, "EVP_PK::create_signer_info: Error processing cert from %s .", priv_key_file.c_str());

    BIO_reset(key_bio);  // move file pointer to start of file

    // read in private key
    *evp_priv_key = PEM_read_bio_PrivateKey(key_bio, NULL, 0, NULL);
    FAIL_IF(evp_priv_key == NULL, "EVP_PK::create_signer_info: Error reading private key from  %s .", priv_key_file.c_str());

	// add signer: takes as input our cert, private key and hash function
	*signer_info = CMS_add1_signer(cms, cert, *evp_priv_key, EVP_sha256(), flags);
    FAIL_IF(!*signer_info, "EVP_PK::create_signer_info: Error adding signer");

	// Add the message digest separately
	FAIL_IF(!CMS_signed_add1_attr_by_NID(*signer_info, NID_pkcs9_messageDigest, V_ASN1_OCTET_STRING, digest, digest_len),
		    "EVP_PK::create_signer_info: Error adding message digest");

	log_debug_p(log, "EVP_PK::create_signer_info: hash value (first 32 bytes):  %s", Ciphersuite::buf2str(digest, 32).c_str());

	ret = 0;

	err:
	if (ret<0) {
		log_err_p(log, "EVP_PK::create_signer_info: Failed");
		openssl_errors();
	}

	if (cert)
		X509_free(cert);
	if (key_bio)
		BIO_free(key_bio);

	return ret;
}
   
//----------------------------------------------------------------------
int
write_sig(CMS_ContentInfo*	cms,
		  LocalBuffer&		db_signed)
{
    BIO*		out_bio = NULL;
    u_char*		cms_buf;     	// holds the CMS structure
    size_t    	cms_buf_len;  	// length of the CMS structure
    u_char*		buf;
    int 		ret = -1;


    out_bio = BIO_new(BIO_s_mem());
    FAIL_IF(!out_bio, "EVP_PK::write_sig: Error creating bio");

    // output the cms structure to the bio
    FAIL_IF(!i2d_CMS_bio(out_bio, cms),
    	    "EVP_PK::write_sig: Error writing to bio");

    // determine how many bytes are buffered in the bio
    cms_buf_len = BIO_ctrl_pending(out_bio);

    // Now convert the CMS structure to u_char*
    cms_buf = (u_char*)OPENSSL_malloc(cms_buf_len);
    FAIL_IF(!cms_buf, "EVP_PK::write_sig: malloc failure");

    // write the data from bio to cms_buf
    FAIL_IF(BIO_read(out_bio, (u_char*)cms_buf, cms_buf_len) <= 0,
	               "EVP_PK::write_sig: BIO_read failed");

    db_signed.reserve(cms_buf_len);
    db_signed.set_len(cms_buf_len);
    buf = db_signed.buf();

    // copy the data into the final structure
    memcpy(buf, cms_buf, cms_buf_len);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::write_sig: Failed");
    	openssl_errors();
    }

    if (out_bio) BIO_free(out_bio);
    if (cms_buf) OPENSSL_free(cms_buf);

    return ret;
}

//----------------------------------------------------------------------
int
EVP_PK::sign(std::string priv_key_file,
                 const LocalBuffer&		db_digest,  // this data buffer contains the message digest (hash value)
                 LocalBuffer&		db_signed  // this data buffer will contain the digital signature
                 ) 
{

    CMS_SignerInfo*		signer_info = NULL;
    CMS_ContentInfo*	cms = CMS_ContentInfo_new();
    EVP_PKEY*			evp_priv_key=NULL;
    int 				ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    FAIL_IF(0 != create_signer_info(priv_key_file, cms, &signer_info, &evp_priv_key, db_digest),
               "EVP_PK::sign: error creating signer info");

    // compute the signature value
    FAIL_IF(!CMS_SignerInfo_sign(signer_info),
               "EVP_PK::sign: Error creating signature");

    // Output the CMS to db_signed's buffer
    FAIL_IF(0 != write_sig(cms, db_signed),
               "EVP_PK::sign: Error writing signature");

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::sign: Failed");
    	openssl_errors();
    }

    if (cms)
    	CMS_ContentInfo_free(cms);
    if (evp_priv_key)
    	EVP_PKEY_free(evp_priv_key);

    return ret;
}

//----------------------------------------------------------------------
int
EVP_PK::signature_length(std::string priv_key_file,
                             size_t            	digest_len,  // length of the message digest
                             size_t&           	sig_len 	 // this will hold the length of the signature
                             )	
{

    LocalBuffer      	db_digest;
    LocalBuffer     		db_signed;
    CMS_SignerInfo*		signer_info = NULL;
    CMS_ContentInfo*	cms = CMS_ContentInfo_new();
    EVP_PKEY*			evp_priv_key = NULL;
    size_t          	max_sig_size = 0;
    void*				foo;
    u_char*				buf;
    int 				ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    db_digest.reserve(digest_len);
    db_digest.set_len(digest_len);
    buf = db_digest.buf();
    for(u_int i=0;i<digest_len;i++) {
        buf[i] = 0;
    }

    // Create the basic CMS structure we would sign
    FAIL_IF(0 != create_signer_info(priv_key_file, cms, &signer_info, &evp_priv_key, db_digest),
              "EVP_PK::signature_length: error creating signer info");

    // Add the signingTime, just like CMS_SIgnerInfo_sign would
    FAIL_IF(!cms_add1_signingTime(signer_info, NULL),
              "EVP_PK::signature_length: error adding signing Time");
    // Fake a max length signature
    max_sig_size = EVP_PKEY_size(evp_priv_key);
    foo = OPENSSL_malloc(max_sig_size);
    ASN1_STRING_set0(signer_info->signature, foo, max_sig_size);

    // Output the resulting CMS
    FAIL_IF(0 != write_sig(cms, db_signed),
              "EVP_PK::signature_length: Error writing signature");
   
    // The resulting length is the max CMS length
    sig_len = db_signed.len();
    log_debug_p(log, "EVP_PK::signature_length: out_len: %zu", sig_len);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::signature_length: Failed");
    	openssl_errors();
    }

    if (cms)
    	CMS_ContentInfo_free(cms);
    if (evp_priv_key)
    	EVP_PKEY_free(evp_priv_key);

    return ret;
}

//----------------------------------------------------------------------
int
EVP_PK::verify(const LocalBuffer&        enc_data,     // buffer containing the signature
                   LocalBuffer&            md,          // contains message digest (i.e. hash value)
                   std::string      certfile
                   )  
{


    X509*						cert = NULL;
    CMS_ContentInfo*			cms = NULL;
    CMS_SignerInfo*				signer_info = NULL;
    STACK_OF(CMS_SignerInfo)*	sinfos = NULL;
    int							attr_num = 0;
    X509_ATTRIBUTE*				x509_attr = NULL;
    ASN1_TYPE*					attr_value;
    BIO*						in_bio = NULL;
    BIO*						cert_bio = NULL;
    EVP_PKEY*					evpkey = NULL;
    int 						ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    // Create a BIO to hold the data
    in_bio = BIO_new(BIO_s_mem());
    FAIL_IF(!in_bio, "EVP_PK::verify: Error creating BIO");

    // write the data to the BIO
    FAIL_IF(BIO_write(in_bio, enc_data.buf(), enc_data.len()) <= 0,
    	          "EVP_PK::verify: BIO_write failed");

    log_debug_p(log,"EVP_PK::verify: certfile = %s\n", certfile.c_str());

    // open X509 cert
    cert_bio = BIO_new_file(certfile.c_str(), "r");
    FAIL_IF(cert_bio == NULL, "EVP_PK::verify: Error opening cert %s .", certfile.c_str());

    // process X509 cert
    cert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
    FAIL_IF(cert == NULL, "EVP_PK::verify: Error processing cert %s .", certfile.c_str());

    evpkey = X509_get_pubkey(cert);

    // Parse the CMS structure
    cms = d2i_CMS_bio(in_bio, NULL);
    FAIL_IF(!cms, "EVP_PK::verify: Error parsing CMS structure");
    
    sinfos = CMS_get0_SignerInfos(cms);
    FAIL_IF(!sinfos, "EVP_PK::verify: Error extracting signer information");

    // Grab the first signer's info (there should only be one signer)
    signer_info = sk_CMS_SignerInfo_value(sinfos, 0);
    FAIL_IF(!signer_info, "EVP_PK::verify: Error extracting signer information");

    // set the certificate for the signer
    CMS_SignerInfo_set1_signer_cert(signer_info, cert);

    // Verify the signature
    FAIL_IF(!CMS_SignerInfo_verify(signer_info),
                 "EVP_PK::verify: Error verifying signature.");
    log_debug_p(log, "EVP_PK::verify: Signature verified correctly (now check that digests match)");

    // extract the message digest value

    attr_num = CMS_signed_get_attr_by_NID(signer_info, NID_pkcs9_messageDigest, -1);
    FAIL_IF(attr_num < 0, "EVP_PK::verify: Error locating attribute that contains message digest");

    // attr_num is the index of the attribute containing the message digest
    x509_attr = CMS_signed_get_attr(signer_info, attr_num);

    // Check if the attribute was found, and if the attribute was set (i.e. is non-empty)
    FAIL_IF(!x509_attr || x509_attr->single || sk_ASN1_TYPE_num(x509_attr->value.set) != 1,
                    "EVP_PK::verify: Error extracting attribute that contains message digest.");
	 
    // extract the value of the attribute (i.e. the message digest from the signature)     
    attr_value = sk_ASN1_TYPE_value(x509_attr->value.set, 0);
    FAIL_IF(!attr_value || attr_value->type != V_ASN1_OCTET_STRING,
                    "EVP_PK::verify: Error extracting the value from the attribute that contains message digest");

    // check the length of the message digest from the signature
    md.reserve(attr_value->value.octet_string->length);
    md.set_len(attr_value->value.octet_string->length);
    memcpy(md.buf(), attr_value->value.octet_string->data, attr_value->value.octet_string->length);
    log_debug_p(log, "EVP_PK::verify: signature verified correctly!!");
    log_debug_p(log, "EVP_PK::verify: hash value:  %s", Ciphersuite::buf2str(md).c_str());

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "EVP_PK::verify: Failed");
    	openssl_errors();
    }

    if (cms)
    	CMS_ContentInfo_free(cms);
    if (in_bio)
    	BIO_free(in_bio);
    if (cert)
    	X509_free(cert);
    if (cert_bio)
    	BIO_free(cert_bio);
    if (evpkey)
    	EVP_PKEY_free(evpkey);

    return ret;
}


} // namespace dtn

#endif /* BSP_ENABLED */
