#include <stdio.h>
#include <string.h>
#include "sha.h"

static unsigned char *shacomp(alg, fmt, bitstr, bitcnt)
int alg;
int fmt;
unsigned char *bitstr;
unsigned long bitcnt;
{
	SHA *s;
	static unsigned char digest[SHA_MAX_HEX_LEN+1];
	unsigned char *ret = digest;

	if ((s = shaopen(alg)) == NULL)
		return(NULL);
	shawrite(bitstr, bitcnt, s);
	shafinish(s);
	if (fmt == SHA_FMT_RAW)
		memcpy(digest, shadigest(s), s->digestlen);
	else if (fmt == SHA_FMT_HEX)
		strcpy((char *) digest, shahex(s));
	else if (fmt == SHA_FMT_BASE64)
		strcpy((char *) digest, shabase64(s));
	else
		ret = NULL;
	shaclose(s);
	return(ret);
}

#define SHA_DIRECT(type, name, alg, fmt) 			\
type name(bitstr, bitcnt)					\
unsigned char *bitstr;						\
unsigned long bitcnt;						\
{								\
	return((type) shacomp(alg, fmt, bitstr, bitcnt));	\
}

SHA_DIRECT(unsigned char *, sha1digest, SHA1, SHA_FMT_RAW)
SHA_DIRECT(char *, sha1hex, SHA1, SHA_FMT_HEX)
SHA_DIRECT(char *, sha1base64, SHA1, SHA_FMT_BASE64)

SHA_DIRECT(unsigned char *, sha224digest, SHA224, SHA_FMT_RAW)
SHA_DIRECT(char *, sha224hex, SHA224, SHA_FMT_HEX)
SHA_DIRECT(char *, sha224base64, SHA224, SHA_FMT_BASE64)

SHA_DIRECT(unsigned char *, sha256digest, SHA256, SHA_FMT_RAW)
SHA_DIRECT(char *, sha256hex, SHA256, SHA_FMT_HEX)
SHA_DIRECT(char *, sha256base64, SHA256, SHA_FMT_BASE64)

SHA_DIRECT(unsigned char *, sha384digest, SHA384, SHA_FMT_RAW)
SHA_DIRECT(char *, sha384hex, SHA384, SHA_FMT_HEX)
SHA_DIRECT(char *, sha384base64, SHA384, SHA_FMT_BASE64)

SHA_DIRECT(unsigned char *, sha512digest, SHA512, SHA_FMT_RAW)
SHA_DIRECT(char *, sha512hex, SHA512, SHA_FMT_HEX)
SHA_DIRECT(char *, sha512base64, SHA512, SHA_FMT_BASE64)
