/* $OpenBSD: ike.h,v 1.4 2001/10/26 14:14:49 ho Exp $ */

/*
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#define ISAKMP_DOI		0
#define IPSEC_DOI		1

#define PROTO_ISAKMP		1
#define PROTO_IPSEC_AH		2
#define PROTO_IPSEC_ESP		3
#define PROTO_IPCOMP		4

#define IKE_ATTR_ENCRYPTION_ALGORITHM	1
#define IKE_ATTR_HASH_ALGORITHM		2
#define IKE_ATTR_AUTHENTICATION_METHOD	3
#define IKE_ATTR_GROUP_DESC		4
#define IKE_ATTR_GROUP_TYPE		5
#define IKE_ATTR_LIFE_TYPE		11

#define IKE_PROTO_INITIALIZER						\
	{ "RESERVED", "ISAKMP", "IPSEC_AH", "IPSEC_ESP", "IPCOMP",	\
	}

#define IKE_ATTR_ENCRYPT_INITIALIZER					\
	{ "NONE", "DES_CBC", "IDEA_CBC", "BLOWFISH_CBC",		\
	  "RC5_R16_B64_CBC", "3DES_CBC", "CAST_CBC", "AES_CBC",		\
	}
#define IKE_ATTR_HASH_INITIALIZER					\
	{ "NONE", "MD5", "SHA", "TIGER",				\
	  "SHA2_256", "SHA2_384", "SHA2_512",				\
	}
#define IKE_ATTR_AUTH_INITIALIZER					\
	{ "NONE", "PRE_SHARED", "DSS", "RSA_SIG",			\
	  "RSA_ENC", "RSA_ENC_REV",					\
	}
#define IKE_ATTR_GROUP_DESC_INITIALIZER					\
	{ "NONE", "MODP_768", "MODP_1024",				\
	  "E2CN_155", "E2CN_185", "MODP_1536",				\
	}
#define IKE_ATTR_GROUP_INITIALIZER					\
	{ "NONE", "MODP", "ECP", "E2CN",				\
	}
#define IKE_ATTR_SA_DURATION_INITIALIZER				\
	{ "NONE", "SECONDS", "KILOBYTES",				\
	}

#define IKE_ATTR_INITIALIZER						\
	{ "NONE", 			/* 0 (not in RFC) */		\
	  "ENCRYPTION_ALGORITHM", 	/* 1 */				\
	  "HASH_ALGORITHM",		/* 2 */				\
	  "AUTHENTICATION_METHOD",	/* 3 */				\
	  "GROUP_DESCRIPTION",		/* 4 */				\
	  "GROUP_TYPE",			/* 5 */				\
	  "GROUP_PRIME",		/* 6 */				\
	  "GROUP_GENERATOR_1",		/* 7 */				\
	  "GROUP_GENERATOR_2",		/* 8 */				\
	  "GROUP_CURVE_1",		/* 9 */				\
	  "GROUP_CURVE_2",		/* 10 */			\
	  "LIFE_TYPE",			/* 11 */			\
	  "LIFE_DURATION",		/* 12 */			\
	  "PRF",			/* 13 */			\
	  "KEY_LENGTH",			/* 14 */			\
	  "FIELD_SIZE",			/* 15 */			\
	  "GROUP_ORDER",		/* 16 */			\
	}

#define IKE_SITUATION_IDENTITY_ONLY	1
#define IKE_SITUATION_SECRECY		2
#define IKE_SITUATION_INTEGRITY		4
/* Mask is all the above, i.e 1+2+4 = 7 */
#define IKE_SITUATION_MASK		7

#define PAYLOAD_NONE		0
#define PAYLOAD_SA		1
#define PAYLOAD_PROPOSAL	2
#define PAYLOAD_TRANSFORM	3
#define PAYLOAD_KE		4
#define PAYLOAD_ID		5
#define PAYLOAD_CERT		6
#define PAYLOAD_CERTREQUEST	7
#define PAYLOAD_HASH		8
#define PAYLOAD_SIG		9
#define PAYLOAD_NONCE		10
#define PAYLOAD_NOTIFICATION	11
#define PAYLOAD_DELETE		12
#define PAYLOAD_VENDOR		13
#define PAYLOAD_ATTRIBUTE	14

#define IKE_PAYLOAD_TYPES_INITIALIZER			\
	{ "NONE",		/*  0 */		\
	  "SA",			/*  1 */		\
	  "PROPOSAL",		/*  2 */		\
	  "TRANSFORM",		/*  3 */		\
	  "KEY_EXCH",		/*  4 */		\
	  "ID",			/*  5 */		\
	  "CERT",		/*  6 */		\
	  "CERTREQUEST",	/*  7 */		\
	  "HASH",		/*  8 */		\
	  "SIG",		/*  9 */		\
	  "NONCE",		/* 10 */		\
	  "NOTIFICATION",	/* 11 */		\
	  "DELETE",		/* 12 */		\
	  "VENDOR",		/* 13 */		\
	  "ATTRIBUTE",		/* 14 (ikecfg) */	\
	}

/* Exchange types */
#define EXCHANGE_NONE		0
#define EXCHANGE_BASE		1
#define EXCHANGE_ID_PROT	2
#define EXCHANGE_AUTH_ONLY	3
#define EXCHANGE_AGGRESSIVE	4
#define EXCHANGE_INFO		5
#define EXCHANGE_TRANSACTION	6
#define EXCHANGE_QUICK_MODE	32
#define EXCHANGE_NEW_GROUP_MODE	33

/* Exchange types */
#define IKE_EXCHANGE_TYPES_INITIALIZER			\
	{ "NONE",		/* 0 */			\
	  "BASE",		/* 1 */			\
	  "ID_PROT",		/* 2 */			\
	  "AUTH_ONLY",		/* 3 */			\
	  "AGGRESSIVE",		/* 4 */			\
	  "INFO",		/* 5 */			\
	  "TRANSACTION",	/* 6 (ikecfg) */	\
	  /* step up to type 32 with unknowns */	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown",					\
	  "QUICK_MODE",		/* 32 */		\
	  "NEW_GROUP_MODE",	/* 33 */		\
	}

#define FLAGS_ENCRYPTION	1
#define FLAGS_COMMIT		2
#define FLAGS_AUTH_ONLY		4

#define CERT_NONE		0
#define CERT_PKCS		1
#define CERT_PGP		2
#define CERT_DNS		3
#define CERT_X509_SIG		4
#define CERT_X509_KE		5
#define CERT_KERBEROS		6
#define CERT_CRL		7
#define CERT_ARL		8
#define CERT_SPKI		9
#define CERT_X509_ATTR		10

#define NOTIFY_INVALID_PAYLOAD_TYPE		1
#define NOTIFY_DOI_NOT_SUPPORTED		2
#define NOTIFY_SITUATION_NOT_SUPPORTED		3
#define NOTIFY_INVALID_COOKIE			4
#define NOTIFY_INVALID_MAJOR_VERSION		5
#define NOTIFY_INVALID_MINOR_VERSION		6
#define NOTIFY_INVALID_EXCHANGE_TYPE		7
#define NOTIFY_INVALID_FLAGS			8
#define NOTIFY_INVALID_MESSAGE_ID		9
#define NOTIFY_INVALID_PROTOCOL_ID		10
#define NOTIFY_INVALID_SPI			11
#define NOTIFY_INVALID_TRANSFORM_ID		12
#define NOTIFY_ATTRIBUTES_NOT_SUPPORTED		13
#define NOTIFY_NO_PROPOSAL_CHOSEN		14
#define NOTIFY_BAD_PROPOSAL_SYNTAX		15
#define NOTIFY_PAYLOAD_MALFORMED		16
#define NOTIFY_INVALID_KEY_INFORMATION		17
#define NOTIFY_INVALID_ID_INFORMATION		18
#define NOTIFY_INVALID_CERT_ENCODING		19
#define NOTIFY_INVALID_CERTIFICATE		20
#define NOTIFY_CERT_TYPE_UNSUPPORTED		21
#define NOTIFY_INVALID_CERT_AUTHORITY		22
#define NOTIFY_INVALID_HASH_INFORMATION		23
#define NOTIFY_AUTHENTICATION_FAILED		24
#define NOTIFY_INVALID_SIGNATURE		25
#define NOTIFY_ADDRESS_NOTIFICATION		26
#define NOTIFY_NOTIFY_SA_LIFETIME		27
#define NOTIFY_CERTIFICATE_UNAVAILABLE		28
#define NOTIFY_UNSUPPORTED_EXCHANGE_TYPE	29
#define NOTIFY_UNEQUAL_PAYLOAD_LENGTHS		30

#define IKE_NOTIFY_TYPES_INITIALIZER			\
	{ "",						\
	  "INVALID PAYLOAD TYPE",			\
	  "DOI NOT SUPPORTED",				\
	  "SITUATION NOT SUPPORTED",			\
	  "INVALID COOKIE",				\
	  "INVALID MAJOR VERSION",			\
	  "INVALID MINOR VERSION",			\
	  "INVALID EXCHANGE TYPE",			\
	  "INVALID FLAGS",				\
	  "INVALID MESSAGE ID",				\
	  "INVALID PROTOCOL ID",			\
	  "INVALID SPI",				\
	  "INVALID TRANSFORM ID",			\
	  "ATTRIBUTES NOT SUPPORTED",			\
	  "NO PROPOSAL CHOSEN",				\
	  "BAD PROPOSAL SYNTAX",			\
	  "PAYLOAD MALFORMED",				\
	  "INVALID KEY INFORMATION",			\
	  "INVALID ID INFORMATION",			\
	  "INVALID CERT ENCODING",			\
	  "INVALID CERTIFICATE",			\
	  "CERT TYPE UNSUPPORTED",			\
	  "INVALID CERT AUTHORITY",			\
	  "INVALID HASH INFORMATION",			\
	  "AUTHENTICATION FAILED",			\
	  "INVALID SIGNATURE",				\
	  "ADDRESS NOTIFICATION",			\
	  "NOTIFY SA LIFETIME",				\
	  "CERTIFICATE UNAVAILABLE",			\
	  "UNSUPPORTED EXCHANGE TYPE",			\
	  "UNEQUAL PAYLOAD LENGTHS",			\
	}

/* RFC 2407, 4.6.3 */
#define NOTIFY_IPSEC_RESPONDER_LIFETIME	24576
#define NOTIFY_IPSEC_REPLAY_STATUS	24577
#define NOTIFY_IPSEC_INITIAL_CONTACT	24578

#define IPSEC_ID_RESERVED		0
#define IPSEC_ID_IPV4_ADDR		1
#define IPSEC_ID_FQDN			2
#define IPSEC_ID_USER_FQDN		3
#define IPSEC_ID_IPV4_ADDR_SUBNET	4
#define IPSEC_ID_IPV6_ADDR		5
#define IPSEC_ID_IPV6_ADDR_SUBNET	6
#define IPSEC_ID_IPV4_ADDR_RANGE	7
#define IPSEC_ID_IPV6_ADDR_RANGE	8
#define IPSEC_ID_DER_ASN1_DN		9
#define IPSEC_ID_DER_ASN1_GN		10
#define IPSEC_ID_KEY_ID			11

#define IPSEC_ID_TYPE_INITIALIZER			\
	{ "RESERVED",					\
	  "IPV4_ADDR",					\
	  "FQDN",					\
	  "USER_FQDN",					\
	  "IPV4_ADDR_SUBNET",				\
	  "IPV6_ADDR",					\
	  "IPV6_ADDR_SUBNET",				\
	  "IPV4_ADDR_RANGE",				\
	  "IPV6_ADDR_RANGE",				\
	  "DER_ASN1_DN",				\
	  "DER_ASN1_GN",				\
	  "KEY_ID",					\
	}

#define IPSEC_ATTR_SA_LIFE_TYPE			1
#define IPSEC_ATTR_SA_LIFE_DURATION		2
#define IPSEC_ATTR_GROUP_DESCRIPTION		3
#define IPSEC_ATTR_ENCAPSULATION_MODE		4
#define IPSEC_ATTR_AUTHENTICATION_ALGORITHM	5
#define IPSEC_ATTR_KEY_LENGTH			6
#define IPSEC_ATTR_KEY_ROUNDS			7
#define IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE	8
#define IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM	9

#define IPSEC_ATTR_INITIALIZER					\
	{ "NONE", "LIFE_TYPE", "LIFE_DURATION",			\
	  "GROUP_DESCRIPTION", "ENCAPSULATION_MODE",		\
	  "AUTHENTICATION_ALGORITHM", "KEY_LENGTH",		\
	  "KEY_ROUNDS", "COMPRESS_DICTIONARY_SIZE",		\
	  "COMPRESS_PRIVATE_ALGORITHM",				\
	}

#define IPSEC_ATTR_DURATION_INITIALIZER				\
	{ "NONE", "SECONDS", "KILOBYTES",			\
	}
#define IPSEC_ATTR_ENCAP_INITIALIZER				\
	{ "NONE", "TUNNEL", "TRANSPORT",			\
	}
#define IPSEC_ATTR_AUTH_INITIALIZER				\
	{ "NONE", "HMAC_MD5", "HMAC_SHA", "DES_MAC", "KPDK",	\
	  "HMAC_SHA2_256", "HMAC_SHA2_384", "HMAC_SHA2_512",	\
	  "HMAC_RIPEMD",					\
	}
#define IPSEC_AH_INITIALIZER					\
	{ "NONE", "MD5", "SHA", "DES", "SHA2_256", "SHA2_384",	\
	  "SHA2_512", "RIPEMD",					\
	}
#define IPSEC_ESP_INITIALIZER					\
	{ "NONE", "DEV_IV64", "DES", "3DES", "RC5", "IDEA",	\
	  "CAST", "BLOWFISH", "3IDEA", "DES_IV32", "RC4",	\
	  "NULL", "AES",					\
	}
#define IPSEC_ATTR_IPCOMP_INITIALIZER				\
	{ "NONE", "OUI", "DEFLATE", "LZS", "V42BIS",		\
	}

/*
 * IKE mode config. 
 */

#define IKE_CFG_ATTRIBUTE_TYPE_INITIALIZER		\
	{ "RESERVED", "CFG_REQUEST", "CFG_REPLY",	\
	  "CFG_SET", "CFG_ACK",				\
	}

#define IKE_CFG_ATTR_INTERNAL_IP4_ADDRESS		1
#define IKE_CFG_ATTR_INTERNAL_IP4_NETMASK		2
#define IKE_CFG_ATTR_INTERNAL_IP4_DNS			3
#define IKE_CFG_ATTR_INTERNAL_IP4_NBNS			4
#define IKE_CFG_ATTR_INTERNAL_ADDRESS_EXPIRY		5
#define IKE_CFG_ATTR_INTERNAL_IP4_DHCP			6
#define IKE_CFG_ATTR_APPLICATION_VERSION		7
#define IKE_CFG_ATTR_INTERNAL_IP6_ADDRESS		8
#define IKE_CFG_ATTR_INTERNAL_IP6_NETMASK		9
#define IKE_CFG_ATTR_INTERNAL_IP6_DNS			10
#define IKE_CFG_ATTR_INTERNAL_IP6_NBNS			11
#define IKE_CFG_ATTR_INTERNAL_IP6_DHCP			12
#define IKE_CFG_ATTR_INTERNAL_IP4_SUBNET		13
#define IKE_CFG_ATTR_SUPPORTED_ATTRIBUTES		14
#define IKE_CFG_ATTR_INTERNAL_IP6_SUBNET		15

#define IKE_CFG_ATTRIBUTE_INITIALIZER				\
	{ "RESERVED", "INTERNAL_IP4_ADDRESS",			\
	  "INTERNAL_IP4_NETMASK", "INTERNAL_IP4_DNS",		\
	  "INTERNAL_IP4_NBNS", "INTERNAL_ADDRESS_EXPIRY",	\
	  "INTERNAL_IP4_DHCP", "APPLICATION_VERSION",		\
	  "INTERNAL_IP6_ADDRESS", "INTERNAL_IP6_NETMASK",	\
	  "INTERNAL_IP6_DNS", "INTERNAL_IP6_NBNS",		\
	  "INTERNAL_IP6_DHCP", "INTERNAL_IP4_SUBNET",		\
	  "SUPPORTED_ATTRIBUTES", "INTERNAL_IP6_SUBNET",	\
	}
