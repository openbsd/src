/*	$OpenBSD: asn_useful.c,v 1.6 1999/02/26 03:32:50 niklas Exp $	*/
/*	$EOM: asn_useful.c,v 1.10 1999/02/25 16:19:36 niklas Exp $	*/

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

#include <sys/param.h>

#include "sysdep.h"

#include "asn.h"
#include "asn_useful.h"

struct norm_type AlgorithmIdentifier[] = {
  { TAG_OBJECTID, UNIVERSAL, "algorithm", 0, 0 },
  { TAG_ANY, UNIVERSAL, "parameters", 0, 0 },
  { TAG_STOP, UNIVERSAL, 0, 0, 0 }
};

struct norm_type Signed[] = {
  { TAG_RAW, UNIVERSAL, "data", 0, 0},
  SEQ("algorithm", AlgorithmIdentifier),
  { TAG_BITSTRING, UNIVERSAL, "encrypted", 0, 0 },
  { TAG_STOP, UNIVERSAL, 0, 0, 0 }
};

struct norm_type Validity[] = {
  { TAG_UTCTIME, UNIVERSAL, "notBefore", 0, 0 },
  { TAG_UTCTIME, UNIVERSAL, "notAfter", 0, 0 },
  { TAG_STOP, UNIVERSAL, 0, 0, 0 }
};

struct norm_type AttributeValueAssertion[] = {
  { TAG_OBJECTID, UNIVERSAL, "AttributeType", 0, 0 },
  { TAG_ANY, UNIVERSAL, "AttributeValue", 0, 0 },
  { TAG_STOP, UNIVERSAL, 0, 0, 0 }
};

struct norm_type RelativeDistinguishedName[] = {
  SEQ ("AttributeValueAssertion", AttributeValueAssertion),
  { TAG_STOP }
};

/* 
 * For decoding this structure is dynamically resized, we add two Names
 * only for encoding purposes.
 */
struct norm_type RDNSequence[] = {
  SETOF ("RelativeDistinguishedName", RelativeDistinguishedName),
  SETOF ("RelativeDistinguishedName", RelativeDistinguishedName),
  { TAG_STOP }
};

struct norm_type SubjectPublicKeyInfo[] = {
  SEQ ("algorithm", AlgorithmIdentifier),
  { TAG_BITSTRING, UNIVERSAL, "subjectPublicKey", 0, 0 },
  { TAG_STOP }
};

struct norm_type Extension[] = {
  { TAG_OBJECTID, UNIVERSAL, "extnId", 0, 0 },
  { TAG_BOOL, UNIVERSAL, "critical", 0, 0 },
  { TAG_OCTETSTRING, UNIVERSAL, "extnValue", 0, 0 },
  { TAG_STOP }
};

struct norm_type Extensions[] = {
  SEQ ("extension", Extension),
  { TAG_STOP }
};

struct norm_type Certificate[] = {
  /* We need to add an explicit tag, HACK XXX */
  { TAG_INTEGER, ADD_EXP(0, UNIVERSAL), "version", 0, 0 },
  { TAG_INTEGER, UNIVERSAL, "serialNumber", 0, 0 },
  SEQ ("signature", AlgorithmIdentifier),
  SEQOF ("issuer", RDNSequence),
  SEQ ("validity", Validity),
  SEQOF ("subject", RDNSequence),
  SEQ ("subjectPublicKeyInfo", SubjectPublicKeyInfo),
  { TAG_RAW, UNIVERSAL, "extension", 0, 0 },
  { TAG_STOP }
};

struct norm_type DigestInfo[] = {
  SEQ ("digestAlgorithm", AlgorithmIdentifier),
  { TAG_OCTETSTRING, UNIVERSAL, "digest", 0, 0 },
  { TAG_STOP }
};

struct asn_objectid asn_ids[] = {
  { "AttributeType", ASN_ID_ATTRIBUTE_TYPE },
  { "CountryName", ASN_ID_COUNTRY_NAME },
  { "LocalityName", ASN_ID_LOCALITY_NAME },
  { "StateOrProvinceName", ASN_ID_STATE_NAME },
  { "OrganizationName", ASN_ID_ORGANIZATION_NAME },
  { "OrganizationUnitName", ASN_ID_ORGUNIT_NAME },
  { "CommonUnitName", ASN_ID_COMMONUNIT_NAME },
  { "pkcs-1", ASN_ID_PKCS },
  { "rsaEncryption", ASN_ID_RSAENCRYPTION },
  { "md2WithRSAEncryption", ASN_ID_MD2WITHRSAENC },
  { "md4WithRSAEncryption", ASN_ID_MD4WITHRSAENC },
  { "md5WithRSAEncryption", ASN_ID_MD5WITHRSAENC },
  { "md2", ASN_ID_MD2 },
  { "md4", ASN_ID_MD4 },
  { "md5", ASN_ID_MD5 },
  { "emailAddress", ASN_ID_EMAILADDRESS },
  { "id-ce", ASN_ID_CE },
  { "subjectAltName", ASN_ID_SUBJECT_ALT_NAME },
  { "issuerAltName", ASN_ID_ISSUER_ALT_NAME },
  { "basicConstraints", ASN_ID_BASIC_CONSTRAINTS },
  { 0, 0 }
};
