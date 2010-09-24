#include <stdio.h>
#include <string.h>
#include "hmac.h"

static unsigned char *hmaccomp(alg, fmt, bitstr, bitcnt, key, keylen)
int alg;
int fmt;
unsigned char *bitstr;
unsigned long bitcnt;
unsigned char *key;
unsigned int keylen;
{
	HMAC *h;
	static unsigned char digest[SHA_MAX_HEX_LEN+1];
	unsigned char *ret = digest;

	if ((h = hmacopen(alg, key, keylen)) == NULL)
		return(NULL);
	hmacwrite(bitstr, bitcnt, h);
	hmacfinish(h);
	if (fmt == SHA_FMT_RAW)
		memcpy(digest, hmacdigest(h), h->osha->digestlen);
	else if (fmt == SHA_FMT_HEX)
		strcpy((char *) digest, hmachex(h));
	else if (fmt == SHA_FMT_BASE64)
		strcpy((char *) digest, hmacbase64(h));
	else
		ret = NULL;
	hmacclose(h);
	return(ret);
}

#define HMAC_DIRECT(type, name, alg, fmt) 			\
type name(bitstr, bitcnt, key, keylen)				\
unsigned char *bitstr;						\
unsigned long bitcnt;						\
unsigned char *key;						\
unsigned int keylen;						\
{								\
	return((type) hmaccomp(alg, fmt, bitstr, bitcnt,	\
					key, keylen));		\
}

HMAC_DIRECT(unsigned char *, hmac1digest, SHA1, SHA_FMT_RAW)
HMAC_DIRECT(char *, hmac1hex, SHA1, SHA_FMT_HEX)
HMAC_DIRECT(char *, hmac1base64, SHA1, SHA_FMT_BASE64)

HMAC_DIRECT(unsigned char *, hmac224digest, SHA224, SHA_FMT_RAW)
HMAC_DIRECT(char *, hmac224hex, SHA224, SHA_FMT_HEX)
HMAC_DIRECT(char *, hmac224base64, SHA224, SHA_FMT_BASE64)

HMAC_DIRECT(unsigned char *, hmac256digest, SHA256, SHA_FMT_RAW)
HMAC_DIRECT(char *, hmac256hex, SHA256, SHA_FMT_HEX)
HMAC_DIRECT(char *, hmac256base64, SHA256, SHA_FMT_BASE64)

HMAC_DIRECT(unsigned char *, hmac384digest, SHA384, SHA_FMT_RAW)
HMAC_DIRECT(char *, hmac384hex, SHA384, SHA_FMT_HEX)
HMAC_DIRECT(char *, hmac384base64, SHA384, SHA_FMT_BASE64)

HMAC_DIRECT(unsigned char *, hmac512digest, SHA512, SHA_FMT_RAW)
HMAC_DIRECT(char *, hmac512hex, SHA512, SHA_FMT_HEX)
HMAC_DIRECT(char *, hmac512base64, SHA512, SHA_FMT_BASE64)
