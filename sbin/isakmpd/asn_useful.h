/*	$OpenBSD: asn_useful.h,v 1.3 1998/11/17 11:10:07 niklas Exp $	*/
/*	$EOM: asn_useful.h,v 1.6 1998/08/21 13:47:57 provos Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _ASN_USEFUL_H_
#define _ASN_USEFUL_H_

extern struct norm_type AlgorithmIdentifier[];
extern struct norm_type Signed[];
extern struct norm_type Validity[];
extern struct norm_type RDNSequence[];
extern struct norm_type Extensions[];
extern struct norm_type Certificate[];
extern struct norm_type DigestInfo[];

extern struct asn_objectid asn_ids[];

/* Accessing a SIGNED type */
#define ASN_SIGNED(x)		((struct norm_type *)((x)->data))
#define ASN_SIGNED_DATA(x)	(ASN_SIGNED(x)->data)
#define ASN_SIGNED_ALGID(x)	((struct norm_type *)(ASN_SIGNED(x)+1)->data)
#define ASN_SIGNED_ENCRYPTED(x) (ASN_SIGNED(x)+2)->data

#define ASN_SIGNED_ALGORITHM(x) (ASN_SIGNED_ALGID(x)->data)

/* Accessing a Certificate */
#define ASN_NT			struct norm_type *
#define ASN_CERT(x)		((ASN_NT)((x)->data))
#define ASN_CERT_VERSION(x)	ASN_CERT(x)->data
#define ASN_CERT_SN(x)		(ASN_CERT(x)+1)->data
#define ASN_CERT_ALGID(x)      	((ASN_NT)((ASN_CERT(X)+2)->data))
#define ASN_CERT_ALGORITHM(x)	(ASN_CERT_ALGID(x)->data)
#define ASN_CERT_ISSUER(x)	(ASN_CERT(x)+3)
#define ASN_CERT_VALIDITY(x)	(ASN_CERT(x)+4)
#define ASN_CERT_SUBJECT(x)	(ASN_CERT(x)+5)
#define ASN_CERT_PUBLICKEY(x)	(ASN_CERT(x)+6)

/* Accesing type Validity */
#define ASN_VAL_BEGIN(x)	(char *)(((ASN_NT)((x)->data))->data)
#define ASN_VAL_END(x)		(char *)(((ASN_NT)((x)->data)+1)->data)

#define ASN_ID_ATTRIBUTE_TYPE		"2 5 4"
#define ASN_ID_COUNTRY_NAME		ASN_ID_ATTRIBUTE_TYPE" 6"
#define ASN_ID_LOCALITY_NAME		ASN_ID_ATTRIBUTE_TYPE" 7"
#define ASN_ID_STATE_NAME		ASN_ID_ATTRIBUTE_TYPE" 8"
#define ASN_ID_ORGANIZATION_NAME	ASN_ID_ATTRIBUTE_TYPE" 10"
#define ASN_ID_ORGUNIT_NAME		ASN_ID_ATTRIBUTE_TYPE" 11"
#define ASN_ID_COMMONUNIT_NAME		ASN_ID_ATTRIBUTE_TYPE" 3"

#define ASN_ID_PKCS			"1 2 840 113549 1 1"
#define ASN_ID_MD2			"1 2 840 113549 2 2"
#define ASN_ID_MD4			"1 2 840 113549 2 4"
#define ASN_ID_MD5			"1 2 840 113549 2 5"
#define ASN_ID_RSAENCRYPTION		ASN_ID_PKCS" 1"
#define ASN_ID_MD2WITHRSAENC		ASN_ID_PKCS" 2"
#define ASN_ID_MD4WITHRSAENC		ASN_ID_PKCS" 3"
#define ASN_ID_MD5WITHRSAENC		ASN_ID_PKCS" 4"

#define ASN_ID_EMAILADDRESS		"1 2 840 113549 1 9 1"

#define ASN_ID_CE			"2 5 29"
#define ASN_ID_SUBJECT_ALT_NAME		ASN_ID_CE" 17"
#define ASN_ID_ISSUER_ALT_NAME		ASN_ID_CE" 18"
#define ASN_ID_BASIC_CONSTRAINTS       	ASN_ID_CE" 19"
#endif /* _ASN_USEFUL_H_ */
