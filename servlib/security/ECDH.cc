// This all uses tidbits from:
// http://www.mail-archive.com/openssl-dev@openssl.org/msg28042.html
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "security/ECDH.h"
#include "Ciphersuite.h"
#include "cms_structs.h"
#include "openssl/ec.h"
#include "contacts/Link.h"


#define sk_OUR_RecipientInfo_push(st, val) SKM_sk_push(OUR_RecipientInfo, (st), (val))
#define sk_OUR_RecipientInfo_value(st, val) SKM_sk_value(OUR_RecipientInfo, (st), (val))
#define sk_OUR_RecipientEncryptedKey_value(st, val) SKM_sk_value(OUR_RecipientEncryptedKey, (st), (val))

#define M_ASN1_free_of_amy(x, type) \
		ASN1_item_free((ASN1_VALUE*)CHECKED_PTR_OF(type, x), ASN1_ITEM_rptr(type))

DECLARE_ASN1_ITEM(OUR_EnvelopedData)
DECLARE_ASN1_ITEM(OUR_RecipientInfo)
DECLARE_ASN1_ITEM(OUR_KeyAgreeRecipientInfo)
DECLARE_ASN1_ITEM(OUR_OriginatorPublicKey)
DECLARE_ASN1_ITEM(OUR_RecipientEncryptedKey)

typedef struct ECC_OUR_SharedInfo_st ECC_OUR_SharedInfo;

struct ECC_OUR_SharedInfo_st {
    ASN1_OBJECT *oid;
    ASN1_OCTET_STRING *ukm;
    ASN1_OCTET_STRING *suppPubInfo;
};

ASN1_SEQUENCE(ECC_OUR_SharedInfo) = {
    ASN1_SIMPLE(ECC_OUR_SharedInfo, oid, ASN1_OBJECT),
    ASN1_OPT(ECC_OUR_SharedInfo, ukm, ASN1_OCTET_STRING),
    ASN1_SIMPLE(ECC_OUR_SharedInfo, suppPubInfo, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(ECC_OUR_SharedInfo)

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
int ECDH::pad(unsigned char**		out, 
			  const unsigned char*	in,
			  const int 			inlen)
{
    int nbytes_padding = 16- inlen %16;
    int outlen = inlen + nbytes_padding;

    // copy the input data to the output buffer
    *out = (unsigned char *)malloc(outlen);
    memcpy(*out, in, inlen);

    // pad extra bytes with the number of bytes of padding
    for(int i=0;i<nbytes_padding;i++){
        (*out)[inlen+i] = nbytes_padding;
    }

    // return the total length of the output buffer
    return outlen;
}

//----------------------------------------------------------------------
int ECDH::unpad(const unsigned char*	in, 
				const int 				inlen)
{
	// determine the number of bytes of padding
    unsigned char lastbyte = in[inlen-1];

    if(1>lastbyte || 16<lastbyte) {
    	log_err_p(log, "Error removing padding");
    }

    // return length of data without padding
    return inlen - lastbyte;
}

//----------------------------------------------------------------------
unsigned char *ECDH::derive_key(const unsigned char* ukm,         // User Key Material (random data)
								const unsigned char* shared_data, // Output of ECDH algorithm
								int 				 sec_lev)     // security level  (either 128 or 192)
{
    ECC_OUR_SharedInfo*		shared_info = NULL;
    BIO*					out_bio = NULL;
    unsigned char*			shared_info_buf = NULL;
    size_t 					shared_info_len;
    unsigned char*			km=NULL;
    unsigned int 			myconst = 0;
    int 					shared_data_len;

    log_debug_p(log, "ECDH::derive_key: security level: %d", sec_lev);

    if(sec_lev <= 128) {
        myconst = 0x80000000U;
        shared_data_len = 256/8;
    } else{
        myconst = 0x00100000U;
        shared_data_len = 384/8;
    }
    unsigned char *shared_key=NULL;

    shared_info = M_ASN1_new_of(ECC_OUR_SharedInfo);
    shared_info->ukm = M_ASN1_new_of(ASN1_OCTET_STRING);
    if(sec_lev <= 128) {
        shared_info->oid = OBJ_txt2obj("2.16.840.1.101.3.4.1.5", 1);
    } else {
        shared_info->oid = OBJ_txt2obj("2.16.840.1.101.3.4.1.45", 1);
    }
    // Add the User Keying Material (UKM) to the shared info
    ASN1_STRING_set(shared_info->ukm, ukm, 16);
    log_debug_p(log, "ECDH::derive_key: User Keying Material (ukm) :  %s", Ciphersuite::buf2str(ukm, 16).c_str());

    //shared_info->suppPubInfo = M_ASN1_new_of(ASN1_OCTET_STRING);
    ASN1_STRING_set(shared_info->suppPubInfo,  &myconst, 4);

    // write shared_info to a bio
    out_bio = BIO_new(BIO_s_mem());
    i2d_ASN1_bio_stream(out_bio, (ASN1_VALUE *)shared_info, NULL, 0, ASN1_ITEM_rptr(ECC_OUR_SharedInfo));

    // now write the bio to a buffer
    shared_info_len = BIO_ctrl_pending(out_bio);
    shared_info_buf = (unsigned char*)malloc(shared_info_len);
    BIO_read(out_bio, (unsigned char*)shared_info_buf, shared_info_len);
    log_debug_p(log, "ECDH::derive_key: Shared Info buf (first 16 bytes) :  %s", Ciphersuite::buf2str(shared_info_buf,16).c_str());

    // initialize the keying material buffer
    km = (unsigned char*)malloc(4+shared_data_len+shared_info_len);
    for(unsigned int i = 0;i<4+shared_data_len+shared_info_len;i++) {
        km[i] = 0xff;
    }

    // copy the shared data into the keying material
    memcpy(km, shared_data, shared_data_len);

    // Set the counter to 1 in the keying material (network byte order)
    km[shared_data_len] = 0;
    km[shared_data_len+1] = 0;
    km[shared_data_len+2] = 0;
    km[shared_data_len+3] = 1;
    memcpy(km+4+shared_data_len, shared_info_buf, shared_info_len);

    // generate the shared key from the keying material (using either SHA-256 or SHA-384)
    log_debug_p(log, "ECDH::derive_key: keying material (km) length: %zu", (4+shared_data_len+shared_info_len));
    if(sec_lev <= 128) {
        shared_key = (unsigned char*)malloc(SHA256_DIGEST_LENGTH);
        SHA256(km, 4+shared_data_len+shared_info_len, shared_key);
    } else {
        shared_key = (unsigned char*)malloc(SHA384_DIGEST_LENGTH);
        SHA384(km, 4+shared_data_len+shared_info_len, shared_key);
    }
    free(shared_info_buf);
    free(km);
    M_ASN1_free_of_amy(shared_info, ECC_OUR_SharedInfo);
    BIO_free(out_bio);

    return shared_key;
} 

//----------------------------------------------------------------------
X509 *ECDH::get_cert(const char *filename) {
    BIO *cert_bio = NULL;
    X509 *rcert = NULL;
    cert_bio = BIO_new_file(filename, "r"); 	

    if (cert_bio == NULL) {
        return NULL;
    }

    // read in the certificate
    rcert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
    BIO_free(cert_bio);

    
    if (rcert == NULL) {
        return NULL;
    }
    return rcert;
}

//----------------------------------------------------------------------
void ECDH::build_cms(OUR_ContentInfo**	cms,
				     X509*				rcert,		 // recipient's certificate
				     EC_KEY*			ephemeral,	 // our EC public key
				     const EC_GROUP*	group,		 // the EC group for this key
				     unsigned char*		iv_cbc,		 // IV (used for AES-CBC encryption of BEK)
				     unsigned char*		ukm,
				     unsigned char*		wrapped_key, // contains encrypted cek
				     unsigned char* 	content,
				     size_t 			content_len,
				     int 				sec_lev)	  // security level (either 128 or 192)
{

    OUR_RecipientInfo *ri = NULL;
    OUR_KeyAgreeRecipientInfo *kari =NULL;
	OUR_EnvelopedData *env=NULL;
    OUR_RecipientEncryptedKey *rek=NULL;
    X509_ALGOR *alg = NULL;
    size_t ep_len;
    unsigned char *ep_str = NULL;

    ASN1_OCTET_STRING *iv_cbc_encoded;
    ASN1_STRING *packedalg=NULL;

    // fill in some of the fields in the CMS structure
    *cms = OUR_ContentInfo_new();
    (*cms)->d.envelopedData = M_ASN1_new_of(OUR_EnvelopedData);
    (*cms)->d.envelopedData->version = 0;
    (*cms)->d.envelopedData->encryptedContentInfo->contentType =
						OBJ_nid2obj(NID_pkcs7_data);
    ASN1_OBJECT_free((*cms)->contentType);
	(*cms)->contentType = OBJ_nid2obj(NID_pkcs7_enveloped);
    env = (*cms)->d.envelopedData;
    ri = M_ASN1_new_of(OUR_RecipientInfo);
    ri->d.kari = M_ASN1_new_of(OUR_KeyAgreeRecipientInfo);
    ri->type = CMS_RECIPINFO_AGREE;
    kari = ri->d.kari;
    kari->originator->type = 2;
    kari->originator->d.originatorKey = M_ASN1_new_of(OUR_OriginatorPublicKey);
    X509_ALGOR_set0(kari->originator->d.originatorKey->algorithm, 
                    OBJ_txt2obj("1.2.840.10045.2.1", 1), // id-ecPublicKey
                    V_ASN1_NULL, NULL);

    ep_len = EC_POINT_point2oct(group, EC_KEY_get0_public_key(ephemeral), POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
    ep_str = (unsigned char *)malloc(ep_len);
    EC_POINT_point2oct(group, EC_KEY_get0_public_key(ephemeral), POINT_CONVERSION_UNCOMPRESSED, ep_str, ep_len, NULL);

    ASN1_STRING_set(kari->originator->d.originatorKey->publicKey, ep_str, ep_len);
    // This forces openssl to output the entire bit string, even if it
    // has trailing zeros.
	kari->originator->d.originatorKey->publicKey->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT|0x07);
	kari->originator->d.originatorKey->publicKey->flags |=ASN1_STRING_FLAG_BITS_LEFT;
    log_debug_p(log, "ECDH::build_cms: ep_len=%d", ep_len);
    log_debug_p(log, "ECDH::build_cms: ep_str[ep_len-1]=%x", ep_str[ep_len-1]);
    kari->ukm = M_ASN1_new_of(ASN1_OCTET_STRING);
    ASN1_STRING_set(kari->ukm, ukm, 16);
    kari->version = 3; // For whatever reason, the standard says this is always 3

    rek = M_ASN1_new_of(OUR_RecipientEncryptedKey);

    // We'll use issuer and serial number (rather than subject key identifiers)
    rek->rid->type = 0;
    rek->rid->d.issuerAndSerialNumber = M_ASN1_new_of(OUR_IssuerAndSerialNumber);
    X509_NAME_set(&rek->rid->d.issuerAndSerialNumber->issuer,
					X509_get_issuer_name(rcert));
	ASN1_STRING_copy( rek->rid->d.issuerAndSerialNumber->serialNumber,
				X509_get_serialNumber(rcert));
    SKM_sk_push(OUR_RecipientEncryptedKey, (kari->recipientEncryptedKeys), (rek));
    sk_OUR_RecipientInfo_push(env->recipientInfos, ri);
    iv_cbc_encoded = M_ASN1_new_of(ASN1_OCTET_STRING);
    ASN1_STRING_set(iv_cbc_encoded, iv_cbc, 16);

    alg = M_ASN1_new_of(X509_ALGOR);

    X509_ALGOR_set0(alg, OBJ_txt2obj("2.16.840.1.101.3.4.1.5", 1), V_ASN1_NULL, NULL); // AES-128 KeyWrap

    ASN1_item_pack(alg, ASN1_ITEM_rptr(X509_ALGOR), &packedalg);


    X509_ALGOR_set0(kari->keyEncryptionAlgorithm, OBJ_txt2obj("1.3.132.1.11.1", 1),
                            V_ASN1_SEQUENCE, packedalg); // dhSinglePass-stdDH-sha256kdf-scheme

    // Setting up the content encryption key info
    env->encryptedContentInfo->contentType = OBJ_txt2obj("1.2.840.113549.1.7.1",0); //Thi OID is data
    if(sec_lev <= 128) {
    X509_ALGOR_set0(env->encryptedContentInfo->contentEncryptionAlgorithm, OBJ_txt2obj("2.16.840.1.101.3.4.1.2",0),
                            V_ASN1_OCTET_STRING, iv_cbc_encoded); // This OID is AES128-CBC
    } else {
    X509_ALGOR_set0(env->encryptedContentInfo->contentEncryptionAlgorithm, OBJ_txt2obj("2.16.840.1.101.3.4.1.42",0),
                            V_ASN1_OCTET_STRING, iv_cbc_encoded); // This OID is AES256-CBC
    }

    env->encryptedContentInfo->encryptedContent = M_ASN1_new_of(ASN1_OCTET_STRING);
    if(sec_lev <= 128) {
    	log_debug_p(log, "ECDH::build_cms: wrapped_key legnth: 24");
        ASN1_STRING_set(rek->encryptedKey, wrapped_key, 24);
    } else {
    	log_debug_p(log, "ECDH::build_cms: wrapped_key legnth: 40");
        ASN1_STRING_set(rek->encryptedKey, wrapped_key, 40);
    }
    ASN1_STRING_set(env->encryptedContentInfo->encryptedContent, content, content_len);

    free(ep_str);
    M_ASN1_free_of_amy(alg, X509_ALGOR);
 
}

//----------------------------------------------------------------------
int ECDH::encrypt(const LocalBuffer &input,
		 	      const char*			cert_filename,	// filename of the recipient's certificate
                  LocalBuffer &cms_output,
		 	      int 					sec_lev)		// security level (either 128 or 192)
{

    const unsigned char *data_in = input.buf();
    const size_t datalen_in = input.len();
    int 				len;
    int 				ret = -1;
    int 				flags = CMS_BINARY;
    const EC_GROUP*		group=NULL;						  // EC group for the public (ECDH) keys
    X509*				rcert = NULL; 				  // Recipient's certificate
    EC_KEY*				r_pk = NULL; 				  // Recipient's public key (from their certificate), used in ECDH
    EC_KEY*				ephemeral = NULL;			  // Sender's public key (used in the ECDH algorithm)
    OUR_ContentInfo*	cms = NULL;
    BIO*				out_bio = NULL;
    int 				shared_data_len;
    int 				shared_key_len;

    // set the length of the shared data and shared key
    if(sec_lev <= 128) {
        shared_data_len = 256/8;
        shared_key_len = 128/8;
    } else {
        shared_data_len = 384/8;
        shared_key_len = 256/8;
    }

    unsigned char	 	shared_data[shared_data_len]; // Output of ECDH algorithm
    unsigned char*		shared_key = NULL; 			  // Shared key derived via the KDF from shared_data and ukm
    AES_KEY 			shared_key_aes; 			  // Shared key in the form openssl needs
    int 				cek_len; 					  // length (in bytes) of the CEK

    // set the length of the Content Encryption Key (CEK)
    if(sec_lev <= 128) {
        cek_len = 128/8;
    } else {
        cek_len = 256/8;
    }

    unsigned char 		cek[cek_len];  				  // Content Encryption Key (used to AES-CBC encrypt the shared_key = BEK)
    AES_KEY 			cek_aes; 					  // Content encryption key in the form openssl needs
    int 				wrapped_cek_len = cek_len+8;  // length of the wrapped cek
    unsigned char 		wrapped_cek[wrapped_cek_len]; // wrapped (encrypted with shared_key) cek (i.e. output of AES_Key_Wrap)
    unsigned char 		iv_cbc[16], ivcopy[16]; 	  // IV used with the CEK (to AES-CBC encrypt the BEK)
    unsigned char 		ukm[16]; 					  // User Keying Material (ukm): random data used in the KDF
    unsigned char*		data=NULL; 					  // input data, after padding has been added (i.e. the padded BEK)
    int 				data_len; 					  // length of the padded data
    unsigned char*		aes_encrypted_data=NULL;      // will hold the encrypted padded data (i.e. encrypted, padded BEK)
    EC_GROUP *gtemp=NULL;

    memset(shared_data, 0xffff, shared_data_len);
    memset(cek, 0xffff, cek_len);

    EVP_PKEY *pk=NULL;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // read in the certificate
    rcert = get_cert(cert_filename);
    if(!rcert) {
        log_err_p(log, "ECDH::encrypt: Couldn't open cert file %s", cert_filename); 
        goto err;
    }

    // pad the data (i.e. the BEK)
    data_len = pad(&data, (unsigned char*)data_in, datalen_in);

    // get the recipient's public key from their certificate
    pk = X509_get_pubkey(rcert);

    r_pk = EVP_PKEY_get1_EC_KEY(pk);
    if(!r_pk) {
    	log_err_p(log, "ECDH::encrypt: Error reading key from certificate %s", cert_filename);
        goto err;
    }

    //  Generate random Content Encryption Key (CEK)
    if (!RAND_bytes(cek, sizeof cek)) {
    	log_err_p(log, "ECDH::encrypt: Error generating random CEK");
        goto err;
    }

    // Generate random IV (used with CEK)
    if (!RAND_bytes(iv_cbc, sizeof iv_cbc)) {
    	log_err_p(log, "ECDH::encrypt: Error generating random IV");
        goto err;
    }

    // Generate random User Keying Material (used in the Key Derivation Function to derive a key from the shared_data)
    if (!RAND_bytes(ukm, sizeof ukm)) {
    	log_err_p(log, "ECDH::encrypt: Error generating random User Key Material (UKM)");
        goto err;
    }
    log_debug_p(log, "ECDH::encrypt: User Keying Material (ukm) :  %s", Ciphersuite::buf2str(ukm, 16).c_str());


    // Generate a one-time (ephemeral) elliptic curve public key
    ephemeral = EC_KEY_new();
    if(!ephemeral) {
    	log_err_p(log, "ECDH::encrypt: Error creating new EC key");
        goto err;
    }
    group = EC_KEY_get0_group(r_pk);
    if(!group) {
    	log_err_p(log, "ECDH::encrypt: Error extracting EC group from public key");
        goto err;
    }
    if(EC_GROUP_get_degree(group) != sec_lev*2) {
    	log_err_p(log, "ECDH::encrypt: Group is wrong size (degree is %d, should have been %d)",
    			EC_GROUP_get_degree(group), sec_lev*2);
        goto err;
    }
    if(sec_lev == 128) {
        gtemp = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
    } else if(sec_lev == 192) {
        gtemp = EC_GROUP_new_by_curve_name(NID_secp384r1);
    }
    if( (sec_lev == 128 && 0 !=EC_GROUP_cmp(gtemp, group, NULL)) ||
        (sec_lev == 192 && 0 != EC_GROUP_cmp(gtemp, group, NULL))) {
        goto err;
    }
    if(!EC_KEY_set_group(ephemeral, group)) {
    	log_err_p(log, "ECDH::encrypt: Error setting EC group");
        goto err;
    }
    if(!EC_KEY_generate_key(ephemeral)) {
    	log_err_p(log, "ECDH::encrypt: Error generating EC public key");
        goto err;
    }

    // Compute the shared data (i.e. the output of the ECDH algorithm)
    if(!ECDH_compute_key(shared_data, shared_data_len, EC_KEY_get0_public_key(r_pk), ephemeral, NULL)) {
    	log_err_p(log, "ECDH::encrypt: Error computing ECDH shared data");
        goto err;
    }
    if (sec_lev == 128) {
    	log_debug_p(log, "ECDH::encrypt: Shared data (i.e. output of ECDH function) :  %s", Ciphersuite::buf2str(shared_data, 32).c_str());
    } else {
    	log_debug_p(log, "ECDH::encrypt: Shared data (i.e. output of ECDH function) :  %s", Ciphersuite::buf2str(shared_data, 48).c_str());
    }

    // Derive the shared key from the shared data and the ukm
    shared_key = derive_key(ukm, shared_data, sec_lev);

    // Set the encryption key to be the shared_key (derived from the ECDH output)
    if(AES_set_encrypt_key(shared_key, shared_key_len*8, &shared_key_aes) < 0) {
        log_err_p(log, "ECDH::encrypt: Error setting AES-CBC key for AES Key Wrap");
        goto err;
    }
    // Encrypt the AES Key Wrap (i.e. encrypt the CEK with the shared_key)
    len = AES_wrap_key(&shared_key_aes, NULL, wrapped_cek, cek, cek_len);
    if(len == 0) {
    	log_err_p(log, "ECDH::encrypt: Error in AES Key Wrap");
        goto err;
    }

    // Set the Content Encryption Key (CEK) for the data (i.e. the padded BEK)
    if(AES_set_encrypt_key(cek, cek_len*8, &cek_aes)<0) {
    	log_err_p(log, "ECDH::encrypt: Error setting AES encryption key");
        goto err;
    }
    aes_encrypted_data = (unsigned char*)malloc(data_len);
    if(!aes_encrypted_data) {
    	log_err_p(log, "ECDH::encrypt: Error in malloc");
        goto err;
    }
    // Note: AES_cbc_encrypt modifies the IV it is passed
    memcpy(ivcopy, iv_cbc, 16);
    AES_cbc_encrypt((unsigned char*)data, aes_encrypted_data, data_len, &cek_aes, ivcopy, AES_ENCRYPT);

    // Build the CMS structure to hold the keying info
    build_cms(&cms, rcert, ephemeral, group, iv_cbc, ukm, wrapped_cek, aes_encrypted_data, data_len, sec_lev);
    if (!cms)
    {
    	log_err_p(log, "ECDH::encrypt: Error creating CMS structure");
        goto err; 
    }

    // Write the CMS structure to a BIO
    out_bio = BIO_new(BIO_s_mem());
	if(!i2d_ASN1_bio_stream(out_bio, (ASN1_VALUE *)cms, NULL, flags, ASN1_ITEM_rptr(CMS_ContentInfo)))
    {
		log_err_p(log, "ECDH::encrypt: Error writing the CMS structure");
		goto err;
    } 
    cms_output.reserve(BIO_ctrl_pending(out_bio));
    cms_output.set_len(BIO_ctrl_pending(out_bio));

    BIO_read(out_bio, (char*)cms_output.buf(), cms_output.len());   

    ret = 0;

    err:

    if(data) {
        free(data);
    }
    if(out_bio) {
        BIO_free(out_bio);
    }
    if(cms) {
        M_ASN1_free_of_amy(cms, OUR_ContentInfo);
    }
    if(aes_encrypted_data) {
        free(aes_encrypted_data);
    }
    if(shared_key) {
        free(shared_key);
    }
    if(ephemeral) {
        EC_KEY_free(ephemeral);
    }
    if(gtemp) {
        EC_GROUP_free(gtemp);
    }
    if(r_pk) {
        EC_KEY_free(r_pk);
    }
    if(pk) {
        EVP_PKEY_free(pk);
    }
    if(rcert) {
        X509_free(rcert);
    }


    if(ret != 0) {
        ERR_load_crypto_strings();
        ERR_print_errors_fp(stderr);
	}
    return ret;
}


//----------------------------------------------------------------------
int ECDH::decrypt(LocalBuffer &out,
				  const char*			priv_filename,  // filename of private key file
                  const LocalBuffer &cms_in)
{
    OUR_RecipientEncryptedKey*	rek;
    BIO*						cms_data_bio = NULL;
    EC_POINT*					ephemeral = NULL;	// sender's EC public key
    EC_KEY*						myKey_ec = NULL;
    EVP_PKEY*					myKey = NULL;
    const EC_GROUP*				group = NULL;		// elliptic curve group
    unsigned char*				iv_cbc=NULL;				// IV used to AES-CBC encrypt the data with the CEK
    int 						cek_len;			// length of the Content Encryption Key (CEK, used to encrypt the BEK)
    AES_KEY 					cek_aes;			// CEK in the format openssl wants
    OUR_ContentInfo*			cms = NULL;
    OUR_RecipientInfo*			ri = NULL;
    unsigned char*				ukm	= NULL;			// User Keying Material (random data that goes into KDF)
    unsigned char*				shared_key = NULL;	// output of KDF (whose input is the shared_data and ukm)
    AES_KEY 					shared_key_aes;		// shared key in the format openssl wants
    int 						shared_key_len;
    FILE*						priv = NULL;
    int 						shared_data_len;
    int ret=-1;

    // read the CMS structure into a BIO
    cms_data_bio = BIO_new(BIO_s_mem());
    if(!BIO_write(cms_data_bio, cms_in.buf(), cms_in.len())) {
    	log_err_p(log, "ECDH::decrypt: Error creating BIO");
        goto err;
    }
    ASN1_item_d2i_bio(ASN1_ITEM_rptr(OUR_ContentInfo), cms_data_bio, &cms);
    if(!cms) {
    	log_err_p(log, "ECDH::decrypt: Error creating CMS structure");
        goto err;
    }

    ri = sk_OUR_RecipientInfo_value(cms->d.envelopedData->recipientInfos,0);
    if(!ri) {
    	log_err_p(log, "ECDH::decrypt: Error creating RecipientInfo structure");
        goto err;
    }
    rek = sk_OUR_RecipientEncryptedKey_value(ri->d.kari->recipientEncryptedKeys, 0);
    if(!rek) {
    	log_err_p(log, "ECDH::decrypt: Error creating RecipientEncryptedKey structure");
        goto err;
    }

    // open the private key file
    priv = fopen(priv_filename, "rb");
    if(!priv) {
    	log_err_p(log, "ECDH::decrypt: Error opening private key file %s", priv_filename);
        goto err;
    }

    // Read in the private key
    if(!PEM_read_PrivateKey(priv, &myKey, NULL, NULL)) {
    	log_err_p(log, "ECDH::decrypt: Error reading in private key from %s", priv_filename);
        goto err;
    }
    fclose(priv);
    myKey_ec = EVP_PKEY_get1_EC_KEY(myKey);
    if(!myKey_ec) {
    	log_err_p(log, "ECDH::decrypt: Error extracting public key from %s", priv_filename);
        goto err;
    }
    group = EC_KEY_get0_group(myKey_ec);
    if(!group) {
    	log_err_p(log, "ECDH::decrypt: Error extracting EC group from key from %s", priv_filename);
        goto err;
    }

    ephemeral = EC_POINT_new(group);
    if(!ephemeral) {
    	log_err_p(log, "ECDH::decrypt: Error creating new EC point");
        goto err;
    }
    if(!EC_POINT_oct2point(group, ephemeral,
        ASN1_STRING_data(ri->d.kari->originator->d.originatorKey->publicKey), ASN1_STRING_length(ri->d.kari->originator->d.originatorKey->publicKey), NULL)) {
    	log_err_p(log, "ECDH::decrypt: Error reading in elliptic curve point from CMS (length=%d)", ASN1_STRING_length(ri->d.kari->originator->d.originatorKey->publicKey));
        goto err;
    }

    // Check the security level here and set the size of the keys
    log_debug_p(log, "ECDH::decrypt: Degree of the elliptic curve group: %d", EC_GROUP_get_degree(group));
    if(EC_GROUP_get_degree(group) <= 256) {
        shared_data_len = 256/8;
        shared_key_len= 128/8;
        cek_len = 128/8;
    } else {
        shared_data_len = 384/8;
        shared_key_len = 256/8;
        cek_len = 256/8;
    }
    {
        unsigned char shared_data[shared_data_len];
        unsigned char cek[cek_len];
        memset(shared_data, 0xffff, shared_data_len);
        memset(cek, 0xffff, cek_len);

        // Retrieve the User Keying Material
        ukm = ASN1_STRING_data(ri->d.kari->ukm);
        log_debug_p(log, "ECDH::decrypt: User Keying Material (ukm) :  %s", Ciphersuite::buf2str(ukm, 16).c_str());

        // Compute the ECDH shared value from the public key and my private key
        if(!ECDH_compute_key(shared_data, shared_data_len, ephemeral, myKey_ec, NULL)) {
            log_err_p(log, "ECDH::decrypt: Error computing ECDH shared value");
            goto err;
        }

        // Derive the shared key from the shared data and the ukm
        shared_key = derive_key(ukm, shared_data, shared_data_len*4);
        if(AES_set_decrypt_key(shared_key, shared_key_len*8, &shared_key_aes)<0) {
            log_err_p(log, "ECDH::decrypt: Error setting AES decryption key");
            goto err;
        }

        // iv is the parameter of env->encryptedContentInfo->contentEncryptionAlgorithm
        void *ivcopy;
        int len;
        X509_ALGOR_get0(NULL, &len, &ivcopy,
                cms->d.envelopedData->encryptedContentInfo->contentEncryptionAlgorithm);
        if(!ivcopy) {
            log_err_p(log, "ECDH::decrypt: Error extracting IV");
            goto err;
        }
        iv_cbc = ASN1_STRING_data((ASN1_STRING *)ivcopy);

        for(int i=0;i<16;i++) {
            cek[i] = i;
        }
        if(!AES_unwrap_key(&shared_key_aes, NULL, cek, ASN1_STRING_data(rek->encryptedKey),
                    ASN1_STRING_length(rek->encryptedKey))) {
            log_err_p(log, "ECDH::decrypt: Error in AES_unwrap_key");
            goto err;
        }

        if(AES_set_decrypt_key(cek, cek_len*8, &cek_aes)<0) {
            log_err_p(log, "ECDH::decrypt: Error setting AES decryption key");
            goto err;
        }
    }
    out.reserve(ASN1_STRING_length(cms->d.envelopedData->encryptedContentInfo->encryptedContent));
    out.set_len(ASN1_STRING_length(cms->d.envelopedData->encryptedContentInfo->encryptedContent));
    AES_cbc_encrypt((unsigned char*)ASN1_STRING_data(cms->d.envelopedData->encryptedContentInfo->encryptedContent),
    		out.buf(), ASN1_STRING_length(cms->d.envelopedData->encryptedContentInfo->encryptedContent), &cek_aes, iv_cbc, AES_DECRYPT);
    out.set_len(unpad(out.buf(), ASN1_STRING_length(cms->d.envelopedData->encryptedContentInfo->encryptedContent)));


    ret = 0;
err:
    if(shared_key) {
        free(shared_key);
    }
    if(ephemeral) {
        EC_POINT_free(ephemeral);
    }
    if(cms_data_bio) {
        BIO_free(cms_data_bio);
    }
    if(myKey) {
        EVP_PKEY_free(myKey);
    }
    if(myKey_ec) {
        EC_KEY_free(myKey_ec);
    }
    if(cms) {
        M_ASN1_free_of_amy(cms, OUR_ContentInfo);
    }
    if(ret==-1) {
        log_err_p(log, "ECDH::decrypt: Error ");
    }
    return ret;

}

} // namespace dtn

#endif /* BSP_ENABLED */
