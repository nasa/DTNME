/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef __CMS_STRUCTS__
#define __CMS_STRUCTS__

#ifdef BSP_ENABLED

extern "C" {


#include <openssl/x509v3.h>
#include <openssl/asn1t.h>

// These make these ASN1 items externally available
DECLARE_ASN1_ITEM(OUR_EnvelopedData)
DECLARE_ASN1_ITEM(OUR_Receipt)
DECLARE_ASN1_ITEM(OUR_SignedData)
DECLARE_ASN1_ITEM(OUR_DigestedData)
DECLARE_ASN1_ITEM(OUR_ReceiptsFrom)
DECLARE_ASN1_ITEM(OUR_EncryptedData)
DECLARE_ASN1_ITEM(OUR_KEKIdentifier)
DECLARE_ASN1_ITEM(OUR_RecipientInfo)
DECLARE_ASN1_ITEM(OUR_CompressedData)
DECLARE_ASN1_ITEM(OUR_OriginatorInfo)
DECLARE_ASN1_ITEM(OUR_KEKRecipientInfo)
DECLARE_ASN1_ITEM(OUR_SignerIdentifier)
DECLARE_ASN1_ITEM(OUR_AuthenticatedData)
DECLARE_ASN1_ITEM(OUR_OtherKeyAttribute)
DECLARE_ASN1_ITEM(OUR_CertificateChoices)
DECLARE_ASN1_ITEM(OUR_OtherRecipientInfo)
DECLARE_ASN1_ITEM(OUR_OriginatorPublicKey)
DECLARE_ASN1_ITEM(OUR_EncryptedContentInfo)
DECLARE_ASN1_ITEM(OUR_RevocationInfoChoice)
DECLARE_ASN1_ITEM(OUR_KeyAgreeRecipientInfo)
DECLARE_ASN1_ITEM(OUR_KeyTransRecipientInfo)
DECLARE_ASN1_ITEM(OUR_PasswordRecipientInfo)
DECLARE_ASN1_ITEM(OUR_RecipientEncryptedKey)
DECLARE_ASN1_ITEM(OUR_OtherCertificateFormat)
DECLARE_ASN1_ITEM(OUR_RecipientKeyIdentifier)
DECLARE_ASN1_ITEM(OUR_EncapsulatedContentInfo)
DECLARE_ASN1_ITEM(OUR_OriginatorIdentifierOrKey)
DECLARE_ASN1_ITEM(OUR_OtherRevocationInfoFormat)
DECLARE_ASN1_ITEM(OUR_KeyAgreeRecipientIdentifier)


// Copied from:
/* crypto/cms/cms.h */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */


typedef struct OUR_ContentInfo_st OUR_ContentInfo;
typedef struct OUR_SignerInfo_st OUR_SignerInfo;
typedef struct OUR_CertificateChoices OUR_CertificateChoices;
typedef struct OUR_RevocationInfoChoice_st OUR_RevocationInfoChoice;
typedef struct OUR_RecipientInfo_st OUR_RecipientInfo;
typedef struct OUR_ReceiptRequest_st OUR_ReceiptRequest;
typedef struct OUR_Receipt_st OUR_Receipt;

#define OUR_SIGNERINFO_ISSUER_SERIAL	0
#define OUR_SIGNERINFO_KEYIDENTIFIER	1

#define OUR_RECIPINFO_TRANS		0
#define OUR_RECIPINFO_AGREE		1
#define OUR_RECIPINFO_KEK		2
#define OUR_RECIPINFO_PASS		3
#define OUR_RECIPINFO_OTHER		4


// Copied from:
/* crypto/cms/cms_lcl.h */
// with OUR replaced by OUR to avoid conflicts
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */


#include <openssl/x509.h>

/* Cryptographic message syntax (OUR) structures: taken
 * from RFC3852
 */

/* Forward references */

typedef struct OUR_IssuerAndSerialNumber_st OUR_IssuerAndSerialNumber;
typedef struct OUR_EncapsulatedContentInfo_st OUR_EncapsulatedContentInfo;
typedef struct OUR_SignerIdentifier_st OUR_SignerIdentifier;
typedef struct OUR_SignedData_st OUR_SignedData;
typedef struct OUR_OtherRevocationInfoFormat_st OUR_OtherRevocationInfoFormat;
typedef struct OUR_OriginatorInfo_st OUR_OriginatorInfo;
typedef struct OUR_EncryptedContentInfo_st OUR_EncryptedContentInfo;
typedef struct OUR_EnvelopedData_st OUR_EnvelopedData;
typedef struct OUR_DigestedData_st OUR_DigestedData;
typedef struct OUR_EncryptedData_st OUR_EncryptedData;
typedef struct OUR_AuthenticatedData_st OUR_AuthenticatedData;
typedef struct OUR_CompressedData_st OUR_CompressedData;
typedef struct OUR_OtherCertificateFormat_st OUR_OtherCertificateFormat;
typedef struct OUR_KeyTransRecipientInfo_st OUR_KeyTransRecipientInfo;
typedef struct OUR_OriginatorPublicKey_st OUR_OriginatorPublicKey;
typedef struct OUR_OriginatorIdentifierOrKey_st OUR_OriginatorIdentifierOrKey;
typedef struct OUR_KeyAgreeRecipientInfo_st OUR_KeyAgreeRecipientInfo;
typedef struct OUR_OtherKeyAttribute_st OUR_OtherKeyAttribute;
typedef struct OUR_RecipientKeyIdentifier_st OUR_RecipientKeyIdentifier;
typedef struct OUR_KeyAgreeRecipientIdentifier_st OUR_KeyAgreeRecipientIdentifier;
typedef struct OUR_RecipientEncryptedKey_st OUR_RecipientEncryptedKey;
typedef struct OUR_KEKIdentifier_st OUR_KEKIdentifier;
typedef struct OUR_KEKRecipientInfo_st OUR_KEKRecipientInfo;
typedef struct OUR_PasswordRecipientInfo_st OUR_PasswordRecipientInfo;
typedef struct OUR_OtherRecipientInfo_st OUR_OtherRecipientInfo;
typedef struct OUR_ReceiptsFrom_st OUR_ReceiptsFrom;

struct OUR_ContentInfo_st
	{
	ASN1_OBJECT *contentType;
	union	{
		ASN1_OCTET_STRING *data;
		OUR_SignedData *signedData;
		OUR_EnvelopedData *envelopedData;
		OUR_DigestedData *digestedData;
		OUR_EncryptedData *encryptedData;
		OUR_AuthenticatedData *authenticatedData;
		OUR_CompressedData *compressedData;
		ASN1_TYPE *other;
		/* Other types ... */
		void *otherData;
		} d;
	};

struct OUR_SignedData_st
	{
	long version;
	STACK_OF(X509_ALGOR) *digestAlgorithms;
	OUR_EncapsulatedContentInfo *encapContentInfo;
	STACK_OF(OUR_CertificateChoices) *certificates;
	STACK_OF(OUR_RevocationInfoChoice) *crls;
	STACK_OF(OUR_SignerInfo) *signerInfos;
	};
 
struct OUR_EncapsulatedContentInfo_st
	{
	ASN1_OBJECT *eContentType;
	ASN1_OCTET_STRING *eContent;
	/* Set to 1 if incomplete structure only part set up */
	int partial;
	};

struct OUR_SignerInfo_st
	{
	long version;
	OUR_SignerIdentifier *sid;
	X509_ALGOR *digestAlgorithm;
	STACK_OF(X509_ATTRIBUTE) *signedAttrs;
	X509_ALGOR *signatureAlgorithm;
	ASN1_OCTET_STRING *signature;
	STACK_OF(X509_ATTRIBUTE) *unsignedAttrs;
	/* Signing certificate and key */
	X509 *signer;
	EVP_PKEY *pkey;
	};

struct OUR_SignerIdentifier_st
	{
	int type;
	union	{
		OUR_IssuerAndSerialNumber *issuerAndSerialNumber;
		ASN1_OCTET_STRING *subjectKeyIdentifier;
		} d;
	};

struct OUR_EnvelopedData_st
	{
	long version;
	OUR_OriginatorInfo *originatorInfo;
	STACK_OF(OUR_RecipientInfo) *recipientInfos;
	OUR_EncryptedContentInfo *encryptedContentInfo;
	STACK_OF(X509_ATTRIBUTE) *unprotectedAttrs;
	};

struct OUR_OriginatorInfo_st
	{
	STACK_OF(OUR_CertificateChoices) *certificates;
	STACK_OF(OUR_RevocationInfoChoice) *crls;
	};

struct OUR_EncryptedContentInfo_st
	{
	ASN1_OBJECT *contentType;
	X509_ALGOR *contentEncryptionAlgorithm;
	ASN1_OCTET_STRING *encryptedContent;
	/* Content encryption algorithm and key */
	const EVP_CIPHER *cipher;
	unsigned char *key;
	size_t keylen;
	};

struct OUR_RecipientInfo_st
	{
 	int type;
 	union	{
  	 	OUR_KeyTransRecipientInfo *ktri;
   		OUR_KeyAgreeRecipientInfo *kari;
   		OUR_KEKRecipientInfo *kekri;
		OUR_PasswordRecipientInfo *pwri;
		OUR_OtherRecipientInfo *ori;
		} d;
	};

typedef OUR_SignerIdentifier OUR_RecipientIdentifier;

struct OUR_KeyTransRecipientInfo_st
	{
	long version;
	OUR_RecipientIdentifier *rid;
	X509_ALGOR *keyEncryptionAlgorithm;
	ASN1_OCTET_STRING *encryptedKey;
	/* Recipient Key and cert */
	X509 *recip;
	EVP_PKEY *pkey;
	};

struct OUR_KeyAgreeRecipientInfo_st
	{
	long version;
	OUR_OriginatorIdentifierOrKey *originator;
	ASN1_OCTET_STRING *ukm;
 	X509_ALGOR *keyEncryptionAlgorithm;
	STACK_OF(OUR_RecipientEncryptedKey) *recipientEncryptedKeys;
	};

struct OUR_OriginatorIdentifierOrKey_st
	{
	int type;
	union	{
		OUR_IssuerAndSerialNumber *issuerAndSerialNumber;
		ASN1_OCTET_STRING *subjectKeyIdentifier;
		OUR_OriginatorPublicKey *originatorKey;
		} d;
	};

struct OUR_OriginatorPublicKey_st
	{
	X509_ALGOR *algorithm;
	ASN1_BIT_STRING *publicKey;
	};

struct OUR_RecipientEncryptedKey_st
	{
 	OUR_KeyAgreeRecipientIdentifier *rid;
 	ASN1_OCTET_STRING *encryptedKey;
	};

struct OUR_KeyAgreeRecipientIdentifier_st
	{
	int type;
	union	{
		OUR_IssuerAndSerialNumber *issuerAndSerialNumber;
		OUR_RecipientKeyIdentifier *rKeyId;
		} d;
	};

struct OUR_RecipientKeyIdentifier_st
	{
 	ASN1_OCTET_STRING *subjectKeyIdentifier;
 	ASN1_GENERALIZEDTIME *date;
 	OUR_OtherKeyAttribute *other;
	};

struct OUR_KEKRecipientInfo_st
	{
 	long version;
 	OUR_KEKIdentifier *kekid;
 	X509_ALGOR *keyEncryptionAlgorithm;
 	ASN1_OCTET_STRING *encryptedKey;
	/* Extra info: symmetric key to use */
	unsigned char *key;
	size_t keylen;
	};

struct OUR_KEKIdentifier_st
	{
 	ASN1_OCTET_STRING *keyIdentifier;
 	ASN1_GENERALIZEDTIME *date;
 	OUR_OtherKeyAttribute *other;
	};

struct OUR_PasswordRecipientInfo_st
	{
 	long version;
 	X509_ALGOR *keyDerivationAlgorithm;
 	X509_ALGOR *keyEncryptionAlgorithm;
 	ASN1_OCTET_STRING *encryptedKey;
	};

struct OUR_OtherRecipientInfo_st
	{
 	ASN1_OBJECT *oriType;
 	ASN1_TYPE *oriValue;
	};

struct OUR_DigestedData_st
	{
	long version;
	X509_ALGOR *digestAlgorithm;
	OUR_EncapsulatedContentInfo *encapContentInfo;
	ASN1_OCTET_STRING *digest;
	};

struct OUR_EncryptedData_st
	{
	long version;
	OUR_EncryptedContentInfo *encryptedContentInfo;
	STACK_OF(X509_ATTRIBUTE) *unprotectedAttrs;
	};

struct OUR_AuthenticatedData_st
	{
	long version;
	OUR_OriginatorInfo *originatorInfo;
	STACK_OF(OUR_RecipientInfo) *recipientInfos;
	X509_ALGOR *macAlgorithm;
	X509_ALGOR *digestAlgorithm;
	OUR_EncapsulatedContentInfo *encapContentInfo;
	STACK_OF(X509_ATTRIBUTE) *authAttrs;
	ASN1_OCTET_STRING *mac;
	STACK_OF(X509_ATTRIBUTE) *unauthAttrs;
	};

struct OUR_CompressedData_st
	{
	long version;
	X509_ALGOR *compressionAlgorithm;
	STACK_OF(OUR_RecipientInfo) *recipientInfos;
	OUR_EncapsulatedContentInfo *encapContentInfo;
	};

struct OUR_RevocationInfoChoice_st
	{
	int type;
	union	{
		X509_CRL *crl;
		OUR_OtherRevocationInfoFormat *other;
		} d;
	};

#define OUR_REVCHOICE_CRL		0
#define OUR_REVCHOICE_OTHER		1

struct OUR_OtherRevocationInfoFormat_st
	{
	ASN1_OBJECT *otherRevInfoFormat;
 	ASN1_TYPE *otherRevInfo;
	};

struct OUR_CertificateChoices
	{
	int type;
		union {
		X509 *certificate;
		ASN1_STRING *extendedCertificate;	/* Obsolete */
		ASN1_STRING *v1AttrCert;	/* Left encoded for now */
		ASN1_STRING *v2AttrCert;	/* Left encoded for now */
		OUR_OtherCertificateFormat *other;
		} d;
	};

#define OUR_CERTCHOICE_CERT		0
#define OUR_CERTCHOICE_EXCERT		1
#define OUR_CERTCHOICE_V1ACERT		2
#define OUR_CERTCHOICE_V2ACERT		3
#define OUR_CERTCHOICE_OTHER		4

struct OUR_OtherCertificateFormat_st
	{
	ASN1_OBJECT *otherCertFormat;
 	ASN1_TYPE *otherCert;
	};

/* This is also defined in pkcs7.h but we duplicate it
 * to allow the OUR code to be independent of PKCS#7
 */

struct OUR_IssuerAndSerialNumber_st
	{
	X509_NAME *issuer;
	ASN1_INTEGER *serialNumber;
	};

struct OUR_OtherKeyAttribute_st
	{
	ASN1_OBJECT *keyAttrId;
 	ASN1_TYPE *keyAttr;
	};

/* ESS structures */

#ifdef HEADER_X509V3_H

struct OUR_ReceiptRequest_st
	{
	ASN1_OCTET_STRING *signedContentIdentifier;
	OUR_ReceiptsFrom *receiptsFrom;
	STACK_OF(GENERAL_NAMES) *receiptsTo;
	};


struct OUR_ReceiptsFrom_st
	{
	int type;
	union
		{
		long allOrFirstTier;
		STACK_OF(GENERAL_NAMES) *receiptList;
		} d;
	};
#endif

struct OUR_Receipt_st
	{
	long version;
	ASN1_OBJECT *contentType;
	ASN1_OCTET_STRING *signedContentIdentifier;
	ASN1_OCTET_STRING *originatorSignatureValue;
	};

//IMPLEMENT_ASN1_FUNCTIONS(OUR_ContentInfo)

DECLARE_ASN1_ITEM(OUR_SignerInfo)
DECLARE_ASN1_ITEM(OUR_IssuerAndSerialNumber)
DECLARE_ASN1_ITEM(OUR_Attributes_Sign)
DECLARE_ASN1_ITEM(OUR_Attributes_Verify)
DECLARE_ASN1_ALLOC_FUNCTIONS(OUR_IssuerAndSerialNumber)
//IMPLEMENT_ASN1_ALLOC_FUNCTIONS(OUR_IssuerAndSerialNumber)

#define OUR_SIGNERINFO_ISSUER_SERIAL	0
#define OUR_SIGNERINFO_KEYIDENTIFIER	1

#define OUR_RECIPINFO_ISSUER_SERIAL	0
#define OUR_RECIPINFO_KEYIDENTIFIER	1

// Also from cms.h

DECLARE_STACK_OF(OUR_SignerInfo)

DECLARE_ASN1_FUNCTIONS(OUR_ContentInfo)

DECLARE_ASN1_FUNCTIONS(OUR_ReceiptRequest)

DECLARE_ASN1_PRINT_FUNCTION(OUR_ContentInfo)



}


#endif //  BSP_ENABLED

#endif // __CMS_STRUCTS__
