/* $OpenBSD: ike.h,v 1.1 2000/10/03 14:25:47 ho Exp $ */

#define ISAKMP_DOI		0
#define IPSEC_DOI		1

#define PROTO_ISAKMP            1

#define IKE_ATTR_ENCRYPTION_ALGORITHM	1
#define IKE_ATTR_HASH_ALGORITHM		2
#define IKE_ATTR_AUTHENTICATION_METHOD	3
#define IKE_ATTR_GROUP_DESC		4
#define IKE_ATTR_GROUP_TYPE		5
#define IKE_ATTR_LIFE_TYPE		11

#define IKE_ATTR_ENCRYPT_INITIALIZER					\
	{ "NONE", "DES_CBS", "IDEA_CBC", "BLOWFISH_CBC",		\
	  "RC5_R16_B64_CBC", "3DES_CBC", "CAST_CBC",			\
	}
#define IKE_ATTR_HASH_INITIALIZER					\
	{ "NONE", "MD5", "SHA", "TIGER",				\
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
#define PAYLOAD_VENDOR          13

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
	}

/* Exchange types */
#define EXCHANGE_NONE           0
#define EXCHANGE_BASE           1
#define EXCHANGE_ID_PROT        2
#define EXCHANGE_AUTH_ONLY      3
#define EXCHANGE_AGGRESSIVE     4
#define EXCHANGE_INFO           5
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
	  /* step up to type 32 with unknowns */	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown",				\
	  "QUICK_MODE",		/* 32 */		\
	  "NEW_GROUP_MODE",	/* 33 */		\
	}

#define FLAGS_ENCRYPTION	1
#define FLAGS_COMMIT		2
#define FLAGS_AUTH_ONLY		4

#define CERT_NONE               0
#define CERT_PKCS               1
#define CERT_PGP                2
#define CERT_DNS                3
#define CERT_X509_SIG           4
#define CERT_X509_KE            5
#define CERT_KERBEROS           6
#define CERT_CRL                7
#define CERT_ARL                8
#define CERT_SPKI               9
#define CERT_X509_ATTR         10

#define NOTIFY_INVALID_PAYLOAD_TYPE          1
#define NOTIFY_DOI_NOT_SUPPORTED             2
#define NOTIFY_SITUATION_NOT_SUPPORTED       3
#define NOTIFY_INVALID_COOKIE                4
#define NOTIFY_INVALID_MAJOR_VERSION         5
#define NOTIFY_INVALID_MINOR_VERSION         6
#define NOTIFY_INVALID_EXCHANGE_TYPE         7
#define NOTIFY_INVALID_FLAGS                 8
#define NOTIFY_INVALID_MESSAGE_ID            9
#define NOTIFY_INVALID_PROTOCOL_ID           10
#define NOTIFY_INVALID_SPI                   11
#define NOTIFY_INVALID_TRANSFORM_ID          12
#define NOTIFY_ATTRIBUTES_NOT_SUPPORTED      13
#define NOTIFY_NO_PROPOSAL_CHOSEN            14
#define NOTIFY_BAD_PROPOSAL_SYNTAX           15
#define NOTIFY_PAYLOAD_MALFORMED             16
#define NOTIFY_INVALID_KEY_INFORMATION       17
#define NOTIFY_INVALID_ID_INFORMATION        18
#define NOTIFY_INVALID_CERT_ENCODING         19
#define NOTIFY_INVALID_CERTIFICATE           20
#define NOTIFY_CERT_TYPE_UNSUPPORTED         21
#define NOTIFY_INVALID_CERT_AUTHORITY        22
#define NOTIFY_INVALID_HASH_INFORMATION      23
#define NOTIFY_AUTHENTICATION_FAILED         24
#define NOTIFY_INVALID_SIGNATURE             25
#define NOTIFY_ADDRESS_NOTIFICATION          26
#define NOTIFY_NOTIFY_SA_LIFETIME            27
#define NOTIFY_CERTIFICATE_UNAVAILABLE       28
#define NOTIFY_UNSUPPORTED_EXCHANGE_TYPE     29
#define NOTIFY_UNEQUAL_PAYLOAD_LENGTHS       30

#define IKE_NOTIFY_TYPES_INITIALIZER			\
      { "",						\
	"INVALID PAYLOAD TYPE",				\
	"DOI NOT SUPPORTED",				\
	"SITUATION NOT SUPPORTED",			\
	"INVALID COOKIE",				\
	"INVALID MAJOR VERSION",			\
	"INVALID MINOR VERSION",			\
	"INVALID EXCHANGE TYPE",			\
	"INVALID FLAGS",				\
	"INVALID MESSAGE ID",				\
	"INVALID PROTOCOL ID",				\
	"INVALID SPI",					\
	"INVALID TRANSFORM ID",				\
	"ATTRIBUTES NOT SUPPORTED",			\
	"NO PROPOSAL CHOSEN",				\
	"BAD PROPOSAL SYNTAX",				\
	"PAYLOAD MALFORMED",				\
	"INVALID KEY INFORMATION",			\
	"INVALID ID INFORMATION",			\
	"INVALID CERT ENCODING",			\
	"INVALID CERTIFICATE",				\
	"CERT TYPE UNSUPPORTED",			\
	"INVALID CERT AUTHORITY",			\
	"INVALID HASH INFORMATION",			\
	"AUTHENTICATION FAILED",			\
	"INVALID SIGNATURE",				\
	"ADDRESS NOTIFICATION",				\
	"NOTIFY SA LIFETIME",				\
	"CERTIFICATE UNAVAILABLE",			\
	"UNSUPPORTED EXCHANGE TYPE",			\
	"UNEQUAL PAYLOAD LENGTHS",			\
      }
