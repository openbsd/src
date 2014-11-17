#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef SvPVbyte
	#if PERL_REVISION == 5 && PERL_VERSION < 8
		#undef SvPVbyte
		#define SvPVbyte(sv, lp) \
			(sv_utf8_downgrade((sv), 0), SvPV((sv), (lp)))
	#endif
#else
	#define SvPVbyte SvPV
#endif

#include "src/sha.c"

static int ix2alg[] =
	{1,1,1,224,224,224,256,256,256,384,384,384,512,512,512,
	512224,512224,512224,512256,512256,512256};

MODULE = Digest::SHA		PACKAGE = Digest::SHA

PROTOTYPES: ENABLE

#ifndef INT2PTR
#define INT2PTR(p, i) (p) (i)
#endif

#define MAX_WRITE_SIZE 16384

int
shaclose(s)
	SHA *	s
CODE:
	RETVAL = shaclose(s);
	sv_setiv(SvRV(ST(0)), 0);
OUTPUT:
	RETVAL

SHA *
shadup(s)
	SHA *	s

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
	Digest::SHA::sha512224 = 15
	Digest::SHA::sha512224_hex = 16
	Digest::SHA::sha512224_base64 = 17
	Digest::SHA::sha512256 = 18
	Digest::SHA::sha512256_hex = 19
	Digest::SHA::sha512256_base64 = 20
PREINIT:
	int i;
	UCHR *data;
	STRLEN len;
	SHA *state;
	char *result;
PPCODE:
	if ((state = shaopen(ix2alg[ix])) == NULL)
		XSRETURN_UNDEF;
	for (i = 0; i < items; i++) {
		data = (UCHR *) (SvPVbyte(ST(i), len));
		while (len > MAX_WRITE_SIZE) {
			shawrite(data, MAX_WRITE_SIZE << 3, state);
			data += MAX_WRITE_SIZE;
			len  -= MAX_WRITE_SIZE;
		}
		shawrite(data, len << 3, state);
	}
	shafinish(state);
	len = 0;
	if (ix % 3 == 0) {
		result = (char *) digcpy(state);
		len = state->digestlen;
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
	Digest::SHA::hmac_sha512224 = 15
	Digest::SHA::hmac_sha512224_hex = 16
	Digest::SHA::hmac_sha512224_base64 = 17
	Digest::SHA::hmac_sha512256 = 18
	Digest::SHA::hmac_sha512256_hex = 19
	Digest::SHA::hmac_sha512256_base64 = 20
PREINIT:
	int i;
	UCHR *key;
	UCHR *data;
	STRLEN len;
	HMAC *state;
	char *result;
PPCODE:
	key = (UCHR *) (SvPVbyte(ST(items-1), len));
	if ((state = hmacopen(ix2alg[ix], key, len)) == NULL)
		XSRETURN_UNDEF;
	for (i = 0; i < items - 1; i++) {
		data = (UCHR *) (SvPVbyte(ST(i), len));
		while (len > MAX_WRITE_SIZE) {
			hmacwrite(data, MAX_WRITE_SIZE << 3, state);
			data += MAX_WRITE_SIZE;
			len  -= MAX_WRITE_SIZE;
		}
		hmacwrite(data, len << 3, state);
	}
	hmacfinish(state);
	len = 0;
	if (ix % 3 == 0) {
		result = (char *) digcpy(state->osha);
		len = state->osha->digestlen;
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
	if (!sv_isa(self, "Digest::SHA"))
		XSRETURN_UNDEF;
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	result = ix ? state->alg : state->digestlen << 3;
	ST(0) = sv_2mortal(newSViv(result));
	XSRETURN(1);

void
add(self, ...)
	SV *	self
PREINIT:
	int i;
	UCHR *data;
	STRLEN len;
	SHA *state;
PPCODE:
	if (!sv_isa(self, "Digest::SHA"))
		XSRETURN_UNDEF;
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	for (i = 1; i < items; i++) {
		data = (UCHR *) (SvPVbyte(ST(i), len));
		while (len > MAX_WRITE_SIZE) {
			shawrite(data, MAX_WRITE_SIZE << 3, state);
			data += MAX_WRITE_SIZE;
			len  -= MAX_WRITE_SIZE;
		}
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
	if (!sv_isa(self, "Digest::SHA"))
		XSRETURN_UNDEF;
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	shafinish(state);
	len = 0;
	if (ix == 0) {
		result = (char *) digcpy(state);
		len = state->digestlen;
	}
	else if (ix == 1)
		result = shahex(state);
	else
		result = shabase64(state);
	ST(0) = sv_2mortal(newSVpv(result, len));
	sharewind(state);
	XSRETURN(1);

void
_getstate(self)
	SV *	self
PREINIT:
	SHA *state;
	UCHR buf[256];
	UCHR *ptr = buf;
PPCODE:
	if (!sv_isa(self, "Digest::SHA"))
		XSRETURN_UNDEF;
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	memcpy(ptr, digcpy(state), state->alg <= SHA256 ? 32 : 64);
	ptr += state->alg <= SHA256 ? 32 : 64;
	memcpy(ptr, state->block, state->alg <= SHA256 ? 64 : 128);
	ptr += state->alg <= SHA256 ? 64 : 128;
	ptr = w32mem(ptr, state->blockcnt);
	ptr = w32mem(ptr, state->lenhh);
	ptr = w32mem(ptr, state->lenhl);
	ptr = w32mem(ptr, state->lenlh);
	ptr = w32mem(ptr, state->lenll);
	ST(0) = sv_2mortal(newSVpv((char *) buf, ptr - buf));
	XSRETURN(1);

void
_putstate(self, ...)
	SV *	self
PREINIT:
	UINT bc;
	STRLEN len;
	SHA *state;
	UCHR *data;
PPCODE:
	if (!sv_isa(self, "Digest::SHA"))
		XSRETURN_UNDEF;
	state = INT2PTR(SHA *, SvIV(SvRV(SvRV(self))));
	data = (UCHR *) SvPV(ST(1), len);
	if (len != (state->alg <= SHA256 ? 116 : 212))
		XSRETURN_UNDEF;
	data = statecpy(state, data);
	memcpy(state->block, data, state->blocksize >> 3);
	data += (state->blocksize >> 3);
	bc = memw32(data), data += 4;
	if (bc >= (state->alg <= SHA256 ? 512 : 1024))
		XSRETURN_UNDEF;
	state->blockcnt = bc;
	state->lenhh = memw32(data), data += 4;
	state->lenhl = memw32(data), data += 4;
	state->lenlh = memw32(data), data += 4;
	state->lenll = memw32(data);
	XSRETURN(1);
