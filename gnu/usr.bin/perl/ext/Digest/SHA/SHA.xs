#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <src/sha.c>
#include <src/hmac.c>

static int ix2alg[] =
	{1,1,1,224,224,224,256,256,256,384,384,384,512,512,512};

MODULE = Digest::SHA		PACKAGE = Digest::SHA		

PROTOTYPES: ENABLE

#include <src/sha.h>
#include <src/hmac.h>

#ifndef INT2PTR
#define INT2PTR(p, i) (p) (i)
#endif

int
shaclose(s)
	SHA *	s

int
shadump(file, s)
	char *	file
	SHA *	s

SHA *
shadup(s)
	SHA *	s

SHA *
shaload(file)
	char *	file

SHA *
shaopen(alg)
	int	alg

void
sharewind(s)
	SHA *	s

unsigned long
shawrite(bitstr, bitcnt, s)
	unsigned char *	bitstr
	unsigned long	bitcnt
	SHA *	s

void
sha1(...)
ALIAS:
	Digest::SHA::sha1 = 0
	Digest::SHA::sha1_hex = 1
	Digest::SHA::sha1_base64 = 2
	Digest::SHA::sha224 = 3
	Digest::SHA::sha224_hex = 4
	Digest::SHA::sha224_base64 = 5
	Digest::SHA::sha256 = 6
	Digest::SHA::sha256_hex = 7
	Digest::SHA::sha256_base64 = 8
	Digest::SHA::sha384 = 9
	Digest::SHA::sha384_hex = 10
	Digest::SHA::sha384_base64 = 11
	Digest::SHA::sha512 = 12
	Digest::SHA::sha512_hex = 13
	Digest::SHA::sha512_base64 = 14
PREINIT:
	int i;
	unsigned char *data;
	STRLEN len;
	SHA *state;
	char *result;
PPCODE:
	if ((state = shaopen(ix2alg[ix])) == NULL)
		XSRETURN_UNDEF;
	for (i = 0; i < items; i++) {
		data = (unsigned char *) (SvPV(ST(i), len));
		shawrite(data, len << 3, state);
	}
	shafinish(state);
	len = 0;
	if (ix % 3 == 0) {
		result = (char *) shadigest(state);
		len = shadsize(state);
	}
	else if (ix % 3 == 1)
		result = shahex(state);
	else
		result = shabase64(state);
	ST(0) = sv_2mortal(newSVpv(result, len));
	shaclose(state);
	XSRETURN(1);

void
hmac_sha1(...)
ALIAS:
	Digest::SHA::hmac_sha1 = 0
	Digest::SHA::hmac_sha1_hex = 1
	Digest::SHA::hmac_sha1_base64 = 2
	Digest::SHA::hmac_sha224 = 3
	Digest::SHA::hmac_sha224_hex = 4
	Digest::SHA::hmac_sha224_base64 = 5
	Digest::SHA::hmac_sha256 = 6
	Digest::SHA::hmac_sha256_hex = 7
	Digest::SHA::hmac_sha256_base64 = 8
	Digest::SHA::hmac_sha384 = 9
	Digest::SHA::hmac_sha384_hex = 10
	Digest::SHA::hmac_sha384_base64 = 11
	Digest::SHA::hmac_sha512 = 12
	Digest::SHA::hmac_sha512_hex = 13
	Digest::SHA::hmac_sha512_base64 = 14
PREINIT:
	int i;
	unsigned char *key;
	unsigned char *data;
	STRLEN len;
	HMAC *state;
	char *result;
PPCODE:
	key = (unsigned char *) (SvPV(ST(items-1), len));
	if ((state = hmacopen(ix2alg[ix], key, len)) == NULL)
		XSRETURN_UNDEF;
	for (i = 0; i < items - 1; i++) {
		data = (unsigned char *) (SvPV(ST(i), len));
		hmacwrite(data, len << 3, state);
	}
	hmacfinish(state);
	len = 0;
	if (ix % 3 == 0) {
		result = (char *) hmacdigest(state);
		len = shadsize(state->osha);
	}
	else if (ix % 3 == 1)
		result = hmachex(state);
	else
		result = hmacbase64(state);
	ST(0) = sv_2mortal(newSVpv(result, len));
	hmacclose(state);
	XSRETURN(1);

void
hashsize(self)
	SV *	self
ALIAS:
	Digest::SHA::hashsize = 0
	Digest::SHA::algorithm = 1
PREINIT:
	SHA *state;
	int result;
PPCODE:
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	result = shadsize(state) << 3;
	if (ix == 1 && result == 160)
		result = 1;
	ST(0) = sv_2mortal(newSViv(result));
	XSRETURN(1);

void
add(self, ...)
	SV *	self
PREINIT:
	int i;
	unsigned char *data;
	STRLEN len;
	SHA *state;
PPCODE:
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	for (i = 1; i < items; i++) {
		data = (unsigned char *) (SvPV(ST(i), len));
		shawrite(data, len << 3, state);
	}
	XSRETURN(1);

void
digest(self)
	SV *	self
ALIAS:
	Digest::SHA::digest = 0
	Digest::SHA::Hexdigest = 1
	Digest::SHA::B64digest = 2
PREINIT:
	STRLEN len;
	SHA *state;
	char *result;
PPCODE:
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	shafinish(state);
	len = 0;
	if (ix == 0) {
		result = (char *) shadigest(state);
		len = shadsize(state);
	}
	else if (ix == 1)
		result = shahex(state);
	else
		result = shabase64(state);
	ST(0) = sv_2mortal(newSVpv(result, len));
	sharewind(state);
	XSRETURN(1);
