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
#include "security/KeySteward.h"
#include "security/SPD.h"


#include "openssl/cms.h"
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include <openssl/err.h>
#include <bundling/SDNV.h>
#include <exception>
#include <string>
#include <dirent.h>
#include <sys/types.h>



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
	/* Signing certificate and key */
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
KeySteward::encrypt(const Bundle*     b,
                    KeyParameterInfo* kpi,
                    const LinkRef&    link,
                    std::string       security_dest,
                    u_char*           data,         	// the BEK to be encrypted
                    size_t            data_len,     	// length of BEK
                    DataBuffer&       encrypted_data,   // encrypted BEK will go here
                    int csnum)                      	// ciphersuite number
{
	// these arguments are not used, cast as void to get rid of warnings
    (void)b;
    (void)kpi;
    (void)link;
    (void)security_dest;
    
    u_char*   			buf;
    size_t    			size;
    std::string 		certfile;
    X509*				cert = NULL;      // X509 certificate for the destination node
    STACK_OF(X509)*		certchain = NULL; // chain of certificates
    CMS_ContentInfo*	cms = NULL;       // CMS structure
    BIO*				data_bio = NULL;
    BIO*				out_bio = NULL;
    BIO*				cert_bio = NULL;
    int 				flags = CMS_STREAM | CMS_BINARY | CMS_NOCERTS;
    int 				ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    if ( data_len > USHRT_MAX )
        return -1;
    
    if ( data_len < 8 )     //sanity check for minimum key size
        return -1;
  
    log_debug_p(log,"KeySteward::encrypt: input data_len  = %d .", data_len);
    log_debug_p(log,"KeySteward::encrypt: input data (= BEK) (1st 16 bytes) :  0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx %2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    		data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9],
    		data[10], data[11], data[12], data[13], data[14], data[15]);

    OpenSSL_add_all_algorithms();

    if(security_dest.length() == 0) {
    	// security dest not set, use bundle destination
    	certfile = Ciphersuite::config->get_pub_key_enc(b->dest().uri().host().c_str(),csnum);
    }
    else {
    	certfile = Ciphersuite::config->get_pub_key_enc(security_dest.c_str(),csnum);
    }

    cert_bio = BIO_new_file(certfile.c_str(), "r");
    if (cert_bio == NULL) {
    	log_err_p(log,"KeySteward::encrypt: Error opening cert %s .", certfile.c_str());
    	goto err;
    }

    // process X509 cert
    cert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
    if (cert == NULL) {
    	log_err_p(log,"KeySteward::encrypt: Error processing cert %s .", certfile.c_str());
    	goto err;
    }

    // CMS_encrypt() expects a stack of certificates
    certchain = sk_X509_new_null();
    if (!certchain || !sk_X509_push(certchain, cert)) {
    	log_err_p(log,"KeySteward::encrypt: Error STACKing certs.");
    	goto err;
    }	
    // set cert to NULL so it isn't freed up twice.
    cert = NULL;

    // read input data (i.e. BEK) into a BIO
    data_bio = BIO_new_mem_buf(data, data_len);
    if (!data_bio) {
    	log_err_p(log, "KeySteward::encrypt: Error creating memory BIO for input data.");
    	goto err;
    }

    // encrypt the data (i.e. BEK) and put into CMS_EnvelopedData structure
    cms = CMS_encrypt(certchain, data_bio, EVP_des_ede3_cbc(), flags);
    if (!cms) {
    	log_err_p(log, "KeySteward::encrypt: CMS_encrypt: failed.");
    	goto err;
    }
    log_debug_p(log, "KeySteward::encrypt: CMS_encrypt: successful");

    out_bio = BIO_new(BIO_s_mem());
    if (!out_bio) {
    	log_err_p(log, "KeySteward::encrypt: Error creating memory BIO for output.");
    	goto err;
    }

    // write CMS_EnvelopedData structure out in BER format
    if (!i2d_CMS_bio_stream(out_bio, cms, data_bio, flags)) {
    	log_err_p(log,"KeySteward::encrypt: Error writing CMS structure Failed");
    	goto err;
    }    

    // Determine length of CMS_EnvelopedData structure
    size = BIO_ctrl_pending(out_bio);
    log_debug_p(log, "KeySteward::encrypt: CMS_EnvelopedData length: %d bytes",size);

    // Allocate memory and copy BER encoded CMS_EnvelopedData structure into buffer
    encrypted_data.reserve(size);
    encrypted_data.set_len(size);
    buf = encrypted_data.buf();

    if (BIO_read(out_bio, (u_char*)buf, size) <= 0) {
    	log_err_p(log, "KeySteward::encrypt: BIO_read failed.");
    	goto err;
    }

    log_debug_p(log, "KeySteward::encrypt: CMS_EnvelopedData (1st 32 bytes) :  0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    		buf[0], buf[1], buf[2], buf[3],buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],buf[11], buf[12],
    		buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],buf[20], buf[21], buf[22], buf[23],
    		buf[24], buf[25], buf[26],buf[27], buf[28], buf[29], buf[30], buf[31]);

    log_debug_p(log, "KeySteward::encrypt: CMS_EnvelopedData (LAST 32 bytes) :  0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    		buf[size-32], buf[size-31], buf[size-30], buf[size-29],buf[size-28], buf[size-27], buf[size-26],
    		buf[size-25], buf[size-24], buf[size-23], buf[size-22],buf[size-21], buf[size-20], buf[size-19],
    		buf[size-18], buf[size-17], buf[size-16], buf[size-15], buf[size-14], buf[size-13],buf[size-12],
    		buf[size-11], buf[size-10], buf[size-9], buf[size-8], buf[size-7], buf[size-6],buf[size-5],
    		buf[size-4], buf[size-3], buf[size-2], buf[size-1]);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::encrypt: Failed");
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
KeySteward::decrypt(const Bundle* b,
                    std::string   security_src,
                    u_char*       enc_data,     // BER-encoded CMS_EnvelopedData holding BEK
                    size_t        enc_data_len, // length of CMS_EnvelopedData
                    DataBuffer&   data_buf,		// this will contain the decrypted BEK
                    int csnum)					// ciphersuite number
{
	// these arguments are not used, cast as void to get rid of warnings
    (void)b;
    (void)security_src;
    
    size_t  			data_len;
    u_char*				buf;
    CMS_ContentInfo* 	cms = NULL;
    BIO*				out_bio = NULL;
    BIO*				enc_data_bio = NULL;
    BIO*				key_bio = NULL;
    int 				ret = -1;
    std::string 		priv_key_file;
    X509*				cert = NULL;			// our own certificate is input to the decryption function
    EVP_PKEY*			evp_priv_key = NULL;	// our private decryption key


    // openssl initialization
    OpenSSL_add_all_algorithms();

    if (enc_data_len < 2)
        return -1;
 
    log_debug_p(log, "KeySteward::decrypt: enc_data_len: %d bytes", enc_data_len);
    log_debug_p(log, "KeySteward::decrypt: enc_data (= CMS_EnvelopedData) (1st 32 bytes):  0x%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx", enc_data[0], enc_data[1], enc_data[2], enc_data[3], enc_data[4], enc_data[5], enc_data[6],
    		enc_data[7], enc_data[8], enc_data[9], enc_data[10], enc_data[11], enc_data[12], enc_data[13],
    		enc_data[14], enc_data[15],enc_data[16], enc_data[17], enc_data[18], enc_data[19], enc_data[20],
    		enc_data[21], enc_data[22], enc_data[23], enc_data[24], enc_data[25], enc_data[26], enc_data[27],
    		enc_data[28], enc_data[29], enc_data[30], enc_data[31]);

    // read in the encrypted data
    enc_data_bio = BIO_new_mem_buf(enc_data, enc_data_len);
    if (!enc_data_bio) {
	    log_err_p(log, "KeySteward::decrypt: Error creating BIO from encrypted data.");
	    goto err;
    }

    // extract CMS_EnvelopedData structure
    cms = d2i_CMS_bio(enc_data_bio, NULL);
    if (!cms) {
    	log_err_p(log, "KeySteward::decrypt: Error reading in CMS structure.");
    	goto err;
    }

    out_bio = BIO_new(BIO_s_mem());
    if (!out_bio) {
	    log_err_p(log, "KeySteward::decrypt: Error creating memory BIO for decrypted output.");
	    goto err;
    }
  
    priv_key_file = Ciphersuite::config->get_priv_key_dec(b->dest().uri().host().c_str(), csnum);

    key_bio = BIO_new_file(priv_key_file.c_str(), "r"); //open private key file in the form of a BIO
    if (key_bio == NULL) {
    	log_err_p(log,"KeySteward::decrypt: Error opening %s .", priv_key_file.c_str());
    	goto err;
    }

    cert = PEM_read_bio_X509(key_bio, NULL, 0, NULL); // process cert using X509 structure
    if (cert == NULL) {
    	log_err_p(log,"KeySteward::decrypt: Error processing cert from %s .", priv_key_file.c_str());
    	goto err;
    }

    BIO_reset(key_bio);  // move file pointer to start of file

    evp_priv_key = PEM_read_bio_PrivateKey(key_bio, NULL, 0, NULL);  // read in key
    if (evp_priv_key == NULL) {
    	log_err_p(log,"KeySteward::decrypt: Error reading private key from  %s .", priv_key_file.c_str());
    	goto err;
    }

    // decrypt the content in the CMS_EnvelopedData structure
    if (!CMS_decrypt(cms, evp_priv_key, cert, NULL, out_bio, 0)) {
    	log_err_p(log, "KeySteward::decrypt: CMS_decrypt: failed.");
    	goto err;
    }

    // Determine length of the decrypted content
    data_len = BIO_ctrl_pending(out_bio);
    log_debug_p(log, "KeySteward::decrypt: decrypted content len: %d bytes", data_len);

    // Allocate memory and copy decrypted content
    data_buf.reserve(data_len);
    data_buf.set_len(data_len);
    buf = data_buf.buf();
    if (BIO_read(out_bio, (u_char*)buf, data_len) <= 0) {
	    log_err_p(log, "KeySteward::decrypt: BIO_read failed.");
	    goto err;
    }
   
    log_debug_p(log, "KeySteward::decrypt: decrypted content (i.e. BEK) (1st 16 bytes) :  0x%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx %2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],buf[11],
    		buf[12], buf[13], buf[14], buf[15]);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::decrypt: Failed");
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
create_signer_info(const Bundle*		b,
					CMS_ContentInfo*	cms,
					CMS_SignerInfo**	signer_info,
					EVP_PKEY**			evp_priv_key,
					DataBuffer&			db_digest,
					int 				csnum)
{
	size_t    	    	digest_len = db_digest.len();
	u_char*   	    	digest = db_digest.buf();
	X509*           	cert = NULL;
	BIO*				key_bio = NULL;
	std::string 		priv_key_file;
	int 				flags = CMS_NOCERTS | CMS_DETACHED | CMS_NOATTR;
	int 				ret = -1;

	if ( digest_len > USHRT_MAX )
		goto err;

	// this file contains both our private key and our certificate
    priv_key_file = Ciphersuite::config->get_priv_key_sig(b->dest().uri().host().c_str(), csnum);

    // open private key file in the form of a BIO
    key_bio = BIO_new_file(priv_key_file.c_str(), "r");
    if (key_bio == NULL) {
    	log_err_p(log,"KeySteward::create_signer_info: Error opening %s .", priv_key_file.c_str());
    	goto err;
    }

    // process X509 certificate
    cert = PEM_read_bio_X509(key_bio, NULL, 0, NULL);
    if (cert == NULL) {
    	log_err_p(log,"KeySteward::create_signer_info: Error processing cert from %s .", priv_key_file.c_str());
    	goto err;
    }

    BIO_reset(key_bio);  // move file pointer to start of file

    // read in private key
    *evp_priv_key = PEM_read_bio_PrivateKey(key_bio, NULL, 0, NULL);
    if (evp_priv_key == NULL) {
    	log_err_p(log,"KeySteward::create_signer_info: Error reading private key from  %s .", priv_key_file.c_str());
    	goto err;
    }

	// add signer: takes as input our cert, private key and hash function
	*signer_info = CMS_add1_signer(cms, cert, *evp_priv_key, EVP_sha256(), flags);
	if (!*signer_info) {
		log_err_p(log, "KeySteward::create_signer_info: Error adding signer");
		goto err;
	}

	// Add the message digest separately
	if (!CMS_signed_add1_attr_by_NID(*signer_info, NID_pkcs9_messageDigest, V_ASN1_OCTET_STRING, digest, digest_len)) {
		log_err_p(log, "KeySteward::create_signer_info: Error adding message digest");
		goto err;
	}
	log_debug_p(log, "KeySteward::create_signer_info: hash value (first 32 bytes):  0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
			"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
			"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
			digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8],
			digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15], digest[16],
			digest[17], digest[18], digest[19], digest[20], digest[21], digest[22], digest[23], digest[24],
			digest[25], digest[26], digest[27], digest[28], digest[29], digest[30], digest[31]);

	ret = 0;

	err:
	if (ret<0) {
		log_err_p(log, "KeySteward::create_signer_info: Failed");
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
		  DataBuffer&		db_signed)
{
    BIO*		out_bio = NULL;
    u_char*		cms_buf;     	// holds the CMS structure
    size_t    	cms_buf_len;  	// length of the CMS structure
    u_char*		buf;
    int 		ret = -1;


    out_bio = BIO_new(BIO_s_mem());
    if (!out_bio) {
        log_err_p(log, "KeySteward::write_sig: Error creating bio");
        goto err;
    }

    // output the cms structure to the bio
    if (!i2d_CMS_bio(out_bio, cms)) {
    	log_err_p(log,"KeySteward::write_sig: Error writing to bio");
    	goto err;
    }

    // determine how many bytes are buffered in the bio
    cms_buf_len = BIO_ctrl_pending(out_bio);

    // Now convert the CMS structure to u_char*
    cms_buf = (u_char*)OPENSSL_malloc(cms_buf_len);
    if (!cms_buf) {
    	log_err_p(log, "KeySteward::write_sig: malloc failure");
        goto err;
    }

    // write the data from bio to cms_buf
    if (BIO_read(out_bio, (u_char*)cms_buf, cms_buf_len) <= 0) {
	    log_err_p(log, "KeySteward::write_sig: BIO_read failed");
	    goto err;
    }

    db_signed.reserve(cms_buf_len);
    db_signed.set_len(cms_buf_len);
    buf = db_signed.buf();

    // copy the data into the final structure
    memcpy(buf, cms_buf, cms_buf_len);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::write_sig: Failed");
    	openssl_errors();
    }

    if (out_bio) BIO_free(out_bio);
    if (cms_buf) OPENSSL_free(cms_buf);

    return ret;
}

//----------------------------------------------------------------------
int
KeySteward::sign(const Bundle*		b,
                 KeyParameterInfo*	kpi,
                 const LinkRef&		link,
                 DataBuffer&		db_digest,  // this data buffer contains the message digest (hash value)
                 DataBuffer&		db_signed,  // this data buffer will contain the digital signature
                 int                csnum)  	// ciphersuite number
{
	// these arguments are not used, cast as void to get rid of warnings
    (void)kpi;
    (void)link;

    CMS_SignerInfo*		signer_info = NULL;
    CMS_ContentInfo*	cms = CMS_ContentInfo_new();
    EVP_PKEY*			evp_priv_key=NULL;
    int 				ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    if(0 != create_signer_info(b, cms, &signer_info, &evp_priv_key, db_digest,csnum)) {
        log_err_p(log, "KeySteward::sign: error creating signer info");
        goto err;
    }

    // compute the signature value
    if (!CMS_SignerInfo_sign(signer_info)) {
        log_err_p(log, "KeySteward::sign: Error creating signature");
        goto err;
    }

    // Output the CMS to db_signed's buffer
    if(0 != write_sig(cms, db_signed)) {
        log_err_p(log, "KeySteward::sign: Error writing signature");
        goto err;
    }

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::sign: Failed");
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
KeySteward::signature_length(const Bundle*     	b,
                             KeyParameterInfo* 	kpi,
                             const LinkRef&    	link,
                             size_t            	digest_len,  // length of the message digest
                             size_t&           	sig_len, 	 // this will hold the length of the signature
                             int              	csnum)		 // ciphersuite number
{
	// these arguments are not used, cast as void to get rid of warnings
    (void)kpi;
    (void)link;

    DataBuffer      	db_digest;
    DataBuffer     		db_signed;
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
    if(0 != create_signer_info(b, cms, &signer_info, &evp_priv_key, db_digest, csnum)) {
        log_err_p(log, "KeySteward::signature_length: error creating signer info");
        goto err;
    }

    // Add the signingTime, just like CMS_SIgnerInfo_sign would
    if (!cms_add1_signingTime(signer_info, NULL)) {
        log_err_p(log, "KeySteward::signature_length: error adding signing Time");
        goto err;
    }
    // Fake a max length signature
    max_sig_size = EVP_PKEY_size(evp_priv_key);
    foo = OPENSSL_malloc(max_sig_size);
    ASN1_STRING_set0(signer_info->signature, foo, max_sig_size);

    // Output the resulting CMS
    if(0 != write_sig(cms, db_signed)) {
        log_err_p(log, "KeySteward::signature_length: Error writing signature");
        goto err;
    }
   
    // The resulting length is the max CMS length
    sig_len = db_signed.len();
    log_debug_p(log, "KeySteward::signature_length: out_len: %d", sig_len);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::signature_length: Failed");
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
KeySteward::verify(const Bundle  *b,
                   u_char        *enc_data,     // buffer containing the signature
                   size_t        enc_data_len,  // the size of the buffer
                   u_char        **md,          // will contain the message digest (i.e. hash value)
                   size_t        *md_len,       // will contain the size of the message digest
                   int          csnum)          // ciphersuite number
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
    std::string 				certfile;
    EVP_PKEY*					evpkey = NULL;
    int 						ret = -1;

    // openssl initialization
    OpenSSL_add_all_algorithms();

    // Create a BIO to hold the data
    in_bio = BIO_new(BIO_s_mem());
    if (!in_bio) {
    	log_err_p(log, "KeySteward::verify: Error creating BIO");
    	goto err;
    }

    // write the data to the BIO
    if (BIO_write(in_bio, enc_data, enc_data_len) <= 0) {
    	log_err_p(log, "KeySteward::verify: BIO_write failed");
    	goto err;
    }

    // Read in the source node's certificate
    certfile = Ciphersuite::config->get_pub_key_ver(b->source().uri().host().c_str(), csnum);
    log_debug_p(log,"KeySteward::verify: certfile = %s\n", certfile.c_str());

    // open X509 cert
    cert_bio = BIO_new_file(certfile.c_str(), "r");
    if (cert_bio == NULL) {
    	log_err_p(log,"KeySteward::verify: Error opening cert %s .", certfile.c_str());
    	goto err;
    }

    // process X509 cert
    cert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
    if (cert == NULL) {
    	log_err_p(log,"KeySteward::verify: Error processing cert %s .", certfile.c_str());
    	goto err;
    }

    evpkey = X509_get_pubkey(cert);

    // Parse the CMS structure
    cms = d2i_CMS_bio(in_bio, NULL);
    if (!cms) {
        log_err_p(log, "KeySteward::verify: Error parsing CMS structure");
    	goto err;
    }
    
    sinfos = CMS_get0_SignerInfos(cms);
    if (!sinfos) {
        log_err_p(log, "KeySteward::verify: Error extracting signer information");
    	goto err;
    }

    // Grab the first signer's info (there should only be one signer)
    signer_info = sk_CMS_SignerInfo_value(sinfos, 0);
    if (!signer_info) {
        log_err_p(log, "KeySteward::verify: Error extracting signer information");
    	goto err;
    }

    // set the certificate for the signer
    CMS_SignerInfo_set1_signer_cert(signer_info, cert);

    // Verify the signature
    if (!CMS_SignerInfo_verify(signer_info)) {
        log_err_p(log, "KeySteward::verify: Error verifying signature.");
    	goto err;
    }
    log_debug_p(log, "KeySteward::verify: Signature verified correctly (now check that digests match)");

    // extract the message digest value
    if ((attr_num = CMS_signed_get_attr_by_NID(signer_info, NID_pkcs9_messageDigest, -1)) < 0) {
        log_err_p(log, "KeySteward::verify: Error locating attribute that contains message digest");
    	goto err;
    }

    // attr_num is the index of the attribute containing the message digest
    x509_attr = CMS_signed_get_attr(signer_info, attr_num);

    // Check if the attribute was found, and if the attribute was set (i.e. is non-empty)
    if (!x509_attr || x509_attr->single || sk_ASN1_TYPE_num(x509_attr->value.set) != 1) {
        log_err_p(log, "KeySteward::verify: Error extracting attribute that contains message digest.");
    	goto err;
    }
	 
    // extract the value of the attribute (i.e. the message digest from the signature)     
    attr_value = sk_ASN1_TYPE_value(x509_attr->value.set, 0);
    if (!attr_value || attr_value->type != V_ASN1_OCTET_STRING) {
        log_err_p(log, "KeySteward::verify: Error extracting the value from the attribute that contains message digest");
    	goto err;
    }

    // copy the message digest from the signature
    *md_len = attr_value->value.octet_string->length;
    *md = (u_char *)malloc(*md_len);
    memcpy(*md, attr_value->value.octet_string->data, *md_len);
    log_debug_p(log, "KeySteward::verify: signature verified correctly!!");
    log_debug_p(log, "KeySteward::verify: hash value (first 32 bytes):  0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx"
    		"%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx", (*md)[0],
    		(*md)[1], (*md)[2], (*md)[3], (*md)[4], (*md)[5], (*md)[6], (*md)[7], (*md)[8], (*md)[9], (*md)[10],
    		(*md)[11], (*md)[12], (*md)[13], (*md)[14], (*md)[15], (*md)[16], (*md)[17], (*md)[18], (*md)[19],
    		(*md)[20], (*md)[21], (*md)[22], (*md)[23], (*md)[24], (*md)[25], (*md)[26], (*md)[27], (*md)[28],
    		(*md)[29], (*md)[30], (*md)[31]);

    ret = 0;

    err:
    if (ret<0) {
    	log_err_p(log, "KeySteward::verify: Failed");
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
