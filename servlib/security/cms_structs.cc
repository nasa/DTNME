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


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

extern "C" {
#include "cms_structs.h"    
   
// Copied from:

// Copied from:
/* crypto/cms/cms_asn1.c */
// again, replacing OUR with OUR
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

#include <openssl/asn1t.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>


ASN1_SEQUENCE(OUR_IssuerAndSerialNumber) = {
	ASN1_SIMPLE(OUR_IssuerAndSerialNumber, issuer, X509_NAME),
	ASN1_SIMPLE(OUR_IssuerAndSerialNumber, serialNumber, ASN1_INTEGER)
} ASN1_SEQUENCE_END(OUR_IssuerAndSerialNumber)

ASN1_SEQUENCE(OUR_OtherCertificateFormat) = {
	ASN1_SIMPLE(OUR_OtherCertificateFormat, otherCertFormat, ASN1_OBJECT),
	ASN1_OPT(OUR_OtherCertificateFormat, otherCert, ASN1_ANY)
} ASN1_SEQUENCE_END(OUR_OtherCertificateFormat)

ASN1_CHOICE(OUR_CertificateChoices) = {
	ASN1_SIMPLE(OUR_CertificateChoices, d.certificate, X509),
	ASN1_IMP(OUR_CertificateChoices, d.extendedCertificate, ASN1_SEQUENCE, 0),
	ASN1_IMP(OUR_CertificateChoices, d.v1AttrCert, ASN1_SEQUENCE, 1),
	ASN1_IMP(OUR_CertificateChoices, d.v2AttrCert, ASN1_SEQUENCE, 2),
	ASN1_IMP(OUR_CertificateChoices, d.other, OUR_OtherCertificateFormat, 3)
} ASN1_CHOICE_END(OUR_CertificateChoices)

ASN1_CHOICE(OUR_SignerIdentifier) = {
	ASN1_SIMPLE(OUR_SignerIdentifier, d.issuerAndSerialNumber, OUR_IssuerAndSerialNumber),
	ASN1_IMP(OUR_SignerIdentifier, d.subjectKeyIdentifier, ASN1_OCTET_STRING, 0)
} ASN1_CHOICE_END(OUR_SignerIdentifier)

ASN1_NDEF_SEQUENCE(OUR_EncapsulatedContentInfo) = {
	ASN1_SIMPLE(OUR_EncapsulatedContentInfo, eContentType, ASN1_OBJECT),
	ASN1_NDEF_EXP_OPT(OUR_EncapsulatedContentInfo, eContent, ASN1_OCTET_STRING_NDEF, 0)
} ASN1_NDEF_SEQUENCE_END(OUR_EncapsulatedContentInfo)

/* Minor tweak to operation: free up signer key, cert */
static int cms_si_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
							void *exarg)
	{
	if(operation == ASN1_OP_FREE_POST)
		{
		OUR_SignerInfo *si = (OUR_SignerInfo *)*pval;
		if (si->pkey)
			EVP_PKEY_free(si->pkey);
		if (si->signer)
			X509_free(si->signer);
		}
	return 1;
	}

ASN1_SEQUENCE_cb(OUR_SignerInfo, cms_si_cb) = {
	ASN1_SIMPLE(OUR_SignerInfo, version, LONG),
	ASN1_SIMPLE(OUR_SignerInfo, sid, OUR_SignerIdentifier),
	ASN1_SIMPLE(OUR_SignerInfo, digestAlgorithm, X509_ALGOR),
	ASN1_IMP_SET_OF_OPT(OUR_SignerInfo, signedAttrs, X509_ATTRIBUTE, 0),
	ASN1_SIMPLE(OUR_SignerInfo, signatureAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_SignerInfo, signature, ASN1_OCTET_STRING),
	ASN1_IMP_SET_OF_OPT(OUR_SignerInfo, unsignedAttrs, X509_ATTRIBUTE, 1)
} ASN1_SEQUENCE_END_cb(OUR_SignerInfo, OUR_SignerInfo)

ASN1_SEQUENCE(OUR_OtherRevocationInfoFormat) = {
	ASN1_SIMPLE(OUR_OtherRevocationInfoFormat, otherRevInfoFormat, ASN1_OBJECT),
	ASN1_OPT(OUR_OtherRevocationInfoFormat, otherRevInfo, ASN1_ANY)
} ASN1_SEQUENCE_END(OUR_OtherRevocationInfoFormat)

ASN1_CHOICE(OUR_RevocationInfoChoice) = {
	ASN1_SIMPLE(OUR_RevocationInfoChoice, d.crl, X509_CRL),
	ASN1_IMP(OUR_RevocationInfoChoice, d.other, OUR_OtherRevocationInfoFormat, 1)
} ASN1_CHOICE_END(OUR_RevocationInfoChoice)

ASN1_NDEF_SEQUENCE(OUR_SignedData) = {
	ASN1_SIMPLE(OUR_SignedData, version, LONG),
	ASN1_SET_OF(OUR_SignedData, digestAlgorithms, X509_ALGOR),
	ASN1_SIMPLE(OUR_SignedData, encapContentInfo, OUR_EncapsulatedContentInfo),
	ASN1_IMP_SET_OF_OPT(OUR_SignedData, certificates, OUR_CertificateChoices, 0),
	ASN1_IMP_SET_OF_OPT(OUR_SignedData, crls, OUR_RevocationInfoChoice, 1),
	ASN1_SET_OF(OUR_SignedData, signerInfos, OUR_SignerInfo)
} ASN1_NDEF_SEQUENCE_END(OUR_SignedData)

ASN1_SEQUENCE(OUR_OriginatorInfo) = {
	ASN1_IMP_SET_OF_OPT(OUR_OriginatorInfo, certificates, OUR_CertificateChoices, 0),
	ASN1_IMP_SET_OF_OPT(OUR_OriginatorInfo, crls, OUR_RevocationInfoChoice, 1)
} ASN1_SEQUENCE_END(OUR_OriginatorInfo)

ASN1_NDEF_SEQUENCE(OUR_EncryptedContentInfo) = {
	ASN1_SIMPLE(OUR_EncryptedContentInfo, contentType, ASN1_OBJECT),
	ASN1_SIMPLE(OUR_EncryptedContentInfo, contentEncryptionAlgorithm, X509_ALGOR),
	ASN1_IMP_OPT(OUR_EncryptedContentInfo, encryptedContent, ASN1_OCTET_STRING_NDEF, 0)
} ASN1_NDEF_SEQUENCE_END(OUR_EncryptedContentInfo)

ASN1_SEQUENCE(OUR_KeyTransRecipientInfo) = {
	ASN1_SIMPLE(OUR_KeyTransRecipientInfo, version, LONG),
	ASN1_SIMPLE(OUR_KeyTransRecipientInfo, rid, OUR_SignerIdentifier),
	ASN1_SIMPLE(OUR_KeyTransRecipientInfo, keyEncryptionAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_KeyTransRecipientInfo, encryptedKey, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OUR_KeyTransRecipientInfo)

ASN1_SEQUENCE(OUR_OtherKeyAttribute) = {
	ASN1_SIMPLE(OUR_OtherKeyAttribute, keyAttrId, ASN1_OBJECT),
	ASN1_OPT(OUR_OtherKeyAttribute, keyAttr, ASN1_ANY)
} ASN1_SEQUENCE_END(OUR_OtherKeyAttribute)

ASN1_SEQUENCE(OUR_RecipientKeyIdentifier) = {
	ASN1_SIMPLE(OUR_RecipientKeyIdentifier, subjectKeyIdentifier, ASN1_OCTET_STRING),
	ASN1_OPT(OUR_RecipientKeyIdentifier, date, ASN1_GENERALIZEDTIME),
	ASN1_OPT(OUR_RecipientKeyIdentifier, other, OUR_OtherKeyAttribute)
} ASN1_SEQUENCE_END(OUR_RecipientKeyIdentifier)

ASN1_CHOICE(OUR_KeyAgreeRecipientIdentifier) = {
  ASN1_SIMPLE(OUR_KeyAgreeRecipientIdentifier, d.issuerAndSerialNumber, OUR_IssuerAndSerialNumber),
  ASN1_IMP(OUR_KeyAgreeRecipientIdentifier, d.rKeyId, OUR_RecipientKeyIdentifier, 0)
} ASN1_CHOICE_END(OUR_KeyAgreeRecipientIdentifier)

ASN1_SEQUENCE(OUR_RecipientEncryptedKey) = {
	ASN1_SIMPLE(OUR_RecipientEncryptedKey, rid, OUR_KeyAgreeRecipientIdentifier),
	ASN1_SIMPLE(OUR_RecipientEncryptedKey, encryptedKey, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OUR_RecipientEncryptedKey)

ASN1_SEQUENCE(OUR_OriginatorPublicKey) = {
  ASN1_SIMPLE(OUR_OriginatorPublicKey, algorithm, X509_ALGOR),
  ASN1_SIMPLE(OUR_OriginatorPublicKey, publicKey, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(OUR_OriginatorPublicKey)

ASN1_CHOICE(OUR_OriginatorIdentifierOrKey) = {
  ASN1_SIMPLE(OUR_OriginatorIdentifierOrKey, d.issuerAndSerialNumber, OUR_IssuerAndSerialNumber),
  ASN1_IMP(OUR_OriginatorIdentifierOrKey, d.subjectKeyIdentifier, ASN1_OCTET_STRING, 0),
  ASN1_IMP(OUR_OriginatorIdentifierOrKey, d.originatorKey, OUR_OriginatorPublicKey, 1)
} ASN1_CHOICE_END(OUR_OriginatorIdentifierOrKey)

ASN1_SEQUENCE(OUR_KeyAgreeRecipientInfo) = {
	ASN1_SIMPLE(OUR_KeyAgreeRecipientInfo, version, LONG),
	ASN1_EXP(OUR_KeyAgreeRecipientInfo, originator, OUR_OriginatorIdentifierOrKey, 0),
	ASN1_EXP_OPT(OUR_KeyAgreeRecipientInfo, ukm, ASN1_OCTET_STRING, 1),
	ASN1_SIMPLE(OUR_KeyAgreeRecipientInfo, keyEncryptionAlgorithm, X509_ALGOR),
	ASN1_SEQUENCE_OF(OUR_KeyAgreeRecipientInfo, recipientEncryptedKeys, OUR_RecipientEncryptedKey)
} ASN1_SEQUENCE_END(OUR_KeyAgreeRecipientInfo)

ASN1_SEQUENCE(OUR_KEKIdentifier) = {
	ASN1_SIMPLE(OUR_KEKIdentifier, keyIdentifier, ASN1_OCTET_STRING),
	ASN1_OPT(OUR_KEKIdentifier, date, ASN1_GENERALIZEDTIME),
	ASN1_OPT(OUR_KEKIdentifier, other, OUR_OtherKeyAttribute)
} ASN1_SEQUENCE_END(OUR_KEKIdentifier)

ASN1_SEQUENCE(OUR_KEKRecipientInfo) = {
	ASN1_SIMPLE(OUR_KEKRecipientInfo, version, LONG),
	ASN1_SIMPLE(OUR_KEKRecipientInfo, kekid, OUR_KEKIdentifier),
	ASN1_SIMPLE(OUR_KEKRecipientInfo, keyEncryptionAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_KEKRecipientInfo, encryptedKey, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OUR_KEKRecipientInfo)

ASN1_SEQUENCE(OUR_PasswordRecipientInfo) = {
	ASN1_SIMPLE(OUR_PasswordRecipientInfo, version, LONG),
	ASN1_IMP_OPT(OUR_PasswordRecipientInfo, keyDerivationAlgorithm, X509_ALGOR, 0),
	ASN1_SIMPLE(OUR_PasswordRecipientInfo, keyEncryptionAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_PasswordRecipientInfo, encryptedKey, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OUR_PasswordRecipientInfo)

ASN1_SEQUENCE(OUR_OtherRecipientInfo) = {
  ASN1_SIMPLE(OUR_OtherRecipientInfo, oriType, ASN1_OBJECT),
  ASN1_OPT(OUR_OtherRecipientInfo, oriValue, ASN1_ANY)
} ASN1_SEQUENCE_END(OUR_OtherRecipientInfo)

/* Free up RecipientInfo additional data */
static int cms_ri_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
							void *exarg)
	{
	if(operation == ASN1_OP_FREE_PRE)
		{
		OUR_RecipientInfo *ri = (OUR_RecipientInfo *)*pval;
		if (ri->type == OUR_RECIPINFO_TRANS)
			{
			OUR_KeyTransRecipientInfo *ktri = ri->d.ktri;
			if (ktri->pkey)
				EVP_PKEY_free(ktri->pkey);
			if (ktri->recip)
				X509_free(ktri->recip);
			}
		else if (ri->type == OUR_RECIPINFO_KEK)
			{
			OUR_KEKRecipientInfo *kekri = ri->d.kekri;
			if (kekri->key)
				{
				OPENSSL_cleanse(kekri->key, kekri->keylen);
				OPENSSL_free(kekri->key);
				}
			}
		}
	return 1;
	}

ASN1_CHOICE_cb(OUR_RecipientInfo, cms_ri_cb) = {
	ASN1_SIMPLE(OUR_RecipientInfo, d.ktri, OUR_KeyTransRecipientInfo),
	ASN1_IMP(OUR_RecipientInfo, d.kari, OUR_KeyAgreeRecipientInfo, 1),
	ASN1_IMP(OUR_RecipientInfo, d.kekri, OUR_KEKRecipientInfo, 2),
	ASN1_IMP(OUR_RecipientInfo, d.pwri, OUR_PasswordRecipientInfo, 3),
	ASN1_IMP(OUR_RecipientInfo, d.ori, OUR_OtherRecipientInfo, 4)
} ASN1_CHOICE_END_cb(OUR_RecipientInfo, OUR_RecipientInfo, type)

ASN1_NDEF_SEQUENCE(OUR_EnvelopedData) = {
	ASN1_SIMPLE(OUR_EnvelopedData, version, LONG),
	ASN1_IMP_OPT(OUR_EnvelopedData, originatorInfo, OUR_OriginatorInfo, 0),
	ASN1_SET_OF(OUR_EnvelopedData, recipientInfos, OUR_RecipientInfo),
	ASN1_SIMPLE(OUR_EnvelopedData, encryptedContentInfo, OUR_EncryptedContentInfo),
	ASN1_IMP_SET_OF_OPT(OUR_EnvelopedData, unprotectedAttrs, X509_ATTRIBUTE, 1)
} ASN1_NDEF_SEQUENCE_END(OUR_EnvelopedData)

ASN1_NDEF_SEQUENCE(OUR_DigestedData) = {
	ASN1_SIMPLE(OUR_DigestedData, version, LONG),
	ASN1_SIMPLE(OUR_DigestedData, digestAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_DigestedData, encapContentInfo, OUR_EncapsulatedContentInfo),
	ASN1_SIMPLE(OUR_DigestedData, digest, ASN1_OCTET_STRING)
} ASN1_NDEF_SEQUENCE_END(OUR_DigestedData)

ASN1_NDEF_SEQUENCE(OUR_EncryptedData) = {
	ASN1_SIMPLE(OUR_EncryptedData, version, LONG),
	ASN1_SIMPLE(OUR_EncryptedData, encryptedContentInfo, OUR_EncryptedContentInfo),
	ASN1_IMP_SET_OF_OPT(OUR_EncryptedData, unprotectedAttrs, X509_ATTRIBUTE, 1)
} ASN1_NDEF_SEQUENCE_END(OUR_EncryptedData)

ASN1_NDEF_SEQUENCE(OUR_AuthenticatedData) = {
	ASN1_SIMPLE(OUR_AuthenticatedData, version, LONG),
	ASN1_IMP_OPT(OUR_AuthenticatedData, originatorInfo, OUR_OriginatorInfo, 0),
	ASN1_SET_OF(OUR_AuthenticatedData, recipientInfos, OUR_RecipientInfo),
	ASN1_SIMPLE(OUR_AuthenticatedData, macAlgorithm, X509_ALGOR),
	ASN1_IMP(OUR_AuthenticatedData, digestAlgorithm, X509_ALGOR, 1),
	ASN1_SIMPLE(OUR_AuthenticatedData, encapContentInfo, OUR_EncapsulatedContentInfo),
	ASN1_IMP_SET_OF_OPT(OUR_AuthenticatedData, authAttrs, X509_ALGOR, 2),
	ASN1_SIMPLE(OUR_AuthenticatedData, mac, ASN1_OCTET_STRING),
	ASN1_IMP_SET_OF_OPT(OUR_AuthenticatedData, unauthAttrs, X509_ALGOR, 3)
} ASN1_NDEF_SEQUENCE_END(OUR_AuthenticatedData)

ASN1_NDEF_SEQUENCE(OUR_CompressedData) = {
	ASN1_SIMPLE(OUR_CompressedData, version, LONG),
	ASN1_SIMPLE(OUR_CompressedData, compressionAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OUR_CompressedData, encapContentInfo, OUR_EncapsulatedContentInfo),
} ASN1_NDEF_SEQUENCE_END(OUR_CompressedData)

/* This is the ANY DEFINED BY table for the top level ContentInfo structure */

ASN1_ADB_TEMPLATE(cms_default) = ASN1_EXP(OUR_ContentInfo, d.other, ASN1_ANY, 0);


ASN1_ADB(OUR_ContentInfo) = {
	ADB_ENTRY(NID_pkcs7_data, ASN1_NDEF_EXP(OUR_ContentInfo, d.data, ASN1_OCTET_STRING_NDEF, 0)),
	ADB_ENTRY(NID_pkcs7_enveloped, ASN1_NDEF_EXP(OUR_ContentInfo, d.envelopedData, OUR_EnvelopedData, 0)),
} ASN1_ADB_END(OUR_ContentInfo, 0, contentType, 0, &cms_default_tt, NULL);



/* OUR streaming support */
/*
static int cms_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
							void *exarg)
	{
	ASN1_STREAM_ARG *sarg = exarg;
	OUR_ContentInfo *cms = NULL;
	if (pval)
		cms = (OUR_ContentInfo *)*pval;
	else
		return 1;
	switch(operation)
		{

		case ASN1_OP_STREAM_PRE:
		if (CMS_stream(&sarg->boundary, cms) <= 0)
			return 0;
		case ASN1_OP_DETACHED_PRE:
		sarg->ndef_bio = OUR_dataInit(cms, sarg->out);
		if (!sarg->ndef_bio)
			return 0;
		break;

		case ASN1_OP_STREAM_POST:
		case ASN1_OP_DETACHED_POST:
		if (CMS_dataFinal(cms, sarg->ndef_bio) <= 0)
			return 0;
		break;

		}
	return 1;
	}

    */
ASN1_NDEF_SEQUENCE_cb(OUR_ContentInfo, NULL) = {
	ASN1_SIMPLE(OUR_ContentInfo, contentType, ASN1_OBJECT),
	ASN1_ADB_OBJECT(OUR_ContentInfo)
} ASN1_NDEF_SEQUENCE_END_cb(OUR_ContentInfo, OUR_ContentInfo)

IMPLEMENT_ASN1_FUNCTIONS(OUR_ContentInfo)
/* Specials for signed attributes */

/* When signing attributes we want to reorder them to match the sorted
 * encoding.
 */

ASN1_ITEM_TEMPLATE(OUR_Attributes_Sign) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SET_ORDER, 0, OUR_ATTRIBUTES, X509_ATTRIBUTE)
ASN1_ITEM_TEMPLATE_END(OUR_Attributes_Sign)

/* When verifying attributes we need to use the received order. So 
 * we use SEQUENCE OF and tag it to SET OF
 */

ASN1_ITEM_TEMPLATE(OUR_Attributes_Verify) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF | ASN1_TFLG_IMPTAG | ASN1_TFLG_UNIVERSAL,
				V_ASN1_SET, OUR_ATTRIBUTES, X509_ATTRIBUTE)
ASN1_ITEM_TEMPLATE_END(OUR_Attributes_Verify)

#ifdef HEADER_X509V3_H
ASN1_CHOICE(OUR_ReceiptsFrom) = {
  ASN1_IMP(OUR_ReceiptsFrom, d.allOrFirstTier, LONG, 0),
  ASN1_IMP_SEQUENCE_OF(OUR_ReceiptsFrom, d.receiptList, GENERAL_NAMES, 1)
} ASN1_CHOICE_END(OUR_ReceiptsFrom)

ASN1_SEQUENCE(OUR_ReceiptRequest) = {
  ASN1_SIMPLE(OUR_ReceiptRequest, signedContentIdentifier, ASN1_OCTET_STRING),
  ASN1_SIMPLE(OUR_ReceiptRequest, receiptsFrom, OUR_ReceiptsFrom),
  ASN1_SEQUENCE_OF(OUR_ReceiptRequest, receiptsTo, GENERAL_NAMES)
} ASN1_SEQUENCE_END(OUR_ReceiptRequest)

#endif

ASN1_SEQUENCE(OUR_Receipt) = {
  ASN1_SIMPLE(OUR_Receipt, version, LONG),
  ASN1_SIMPLE(OUR_Receipt, contentType, ASN1_OBJECT),
  ASN1_SIMPLE(OUR_Receipt, signedContentIdentifier, ASN1_OCTET_STRING),
  ASN1_SIMPLE(OUR_Receipt, originatorSignatureValue, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OUR_Receipt)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(OUR_IssuerAndSerialNumber)

}


#endif // BSP_ENABLED
