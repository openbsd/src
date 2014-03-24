/* 
 * This library is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 * 
 *  Copyright 1998-2000 Gisle Aas.
 *  Copyright 1995-1996 Neil Winton.
 *  Copyright 1991-1992 RSA Data Security, Inc.
 *
 * This code is derived from Neil Winton's MD5-1.7 Perl module, which in
 * turn is derived from the reference implementation in RFC 1321 which
 * comes with this message:
 *
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifdef __cplusplus
extern "C" {
#endif
#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <sys/types.h>
#include <md5.h>
#ifdef __cplusplus
}
#endif

#ifndef PERL_UNUSED_VAR
# define PERL_UNUSED_VAR(x) ((void)x)
#endif

#ifndef PERL_MAGIC_ext
# define PERL_MAGIC_ext '~'
#endif

#ifndef Newxz
# define Newxz(v,n,t) Newz(0,v,n,t)
#endif

#ifndef SvMAGIC_set
# define SvMAGIC_set(sv, mg) (SvMAGIC(sv) = (mg))
#endif

#ifndef sv_magicext
# define sv_magicext(sv, obj, type, vtbl, name, namlen) \
    THX_sv_magicext(aTHX_ sv, obj, type, vtbl, name, namlen)
static MAGIC *THX_sv_magicext(pTHX_ SV *sv, SV *obj, int type,
    MGVTBL const *vtbl, char const *name, I32 namlen)
{
    MAGIC *mg;
    if (obj || namlen)
	/* exceeded intended usage of this reserve implementation */
	return NULL;
    Newxz(mg, 1, MAGIC);
    mg->mg_virtual = (MGVTBL*)vtbl;
    mg->mg_type = type;
    mg->mg_ptr = (char *)name;
    mg->mg_len = -1;
    (void) SvUPGRADE(sv, SVt_PVMG);
    mg->mg_moremagic = SvMAGIC(sv);
    SvMAGIC_set(sv, mg);
    SvMAGICAL_off(sv);
    mg_magical(sv);
    return mg;
}
#endif

#if PERL_VERSION < 8
# undef SvPVbyte
# define SvPVbyte(sv, lp) (sv_utf8_downgrade((sv), 0), SvPV((sv), (lp)))
#endif

#if defined(USE_ITHREADS) && defined(MGf_DUP)
STATIC int dup_md5_ctx(pTHX_ MAGIC *mg, CLONE_PARAMS *params)
{
    MD5_CTX *new_ctx;
    PERL_UNUSED_VAR(params);
    New(55, new_ctx, 1, MD5_CTX);
    memcpy(new_ctx, mg->mg_ptr, sizeof(MD5_CTX));
    mg->mg_ptr = (char *)new_ctx;
    return 0;
}
#endif

STATIC MGVTBL vtbl_md5 = {
    NULL, /* get */
    NULL, /* set */
    NULL, /* len */
    NULL, /* clear */
    NULL, /* free */
#ifdef MGf_COPY
    NULL, /* copy */
#endif
#ifdef MGf_DUP
# ifdef USE_ITHREADS
    dup_md5_ctx,
# else
    NULL, /* dup */
# endif
#endif
#ifdef MGf_LOCAL
    NULL /* local */
#endif
};

static MD5_CTX* get_md5_ctx(pTHX_ SV* sv)
{
    MAGIC *mg;

    if (!sv_derived_from(sv, "Digest::MD5"))
	croak("Not a reference to a Digest::MD5 object");

    for (mg = SvMAGIC(SvRV(sv)); mg; mg = mg->mg_moremagic) {
	if (mg->mg_type == PERL_MAGIC_ext && mg->mg_virtual == &vtbl_md5) {
	    return (MD5_CTX *)mg->mg_ptr;
	}
    }

    croak("Failed to get MD5_CTX pointer");
    return (MD5_CTX*)0; /* some compilers insist on a return value */
}

static SV * new_md5_ctx(pTHX_ MD5_CTX *context, const char *klass)
{
    SV *sv = newSV(0);
    SV *obj = newRV_noinc(sv);
#ifdef USE_ITHREADS
    MAGIC *mg;
#endif

    sv_bless(obj, gv_stashpv(klass, 0));

#ifdef USE_ITHREADS
    mg =
#endif
	sv_magicext(sv, NULL, PERL_MAGIC_ext, &vtbl_md5, (const char *)context, 0);

#if defined(USE_ITHREADS) && defined(MGf_DUP)
    mg->mg_flags |= MGf_DUP;
#endif

    return obj;
}


static char* hex_16(const unsigned char* from, char* to)
{
    static const char hexdigits[] = "0123456789abcdef";
    const unsigned char *end = from + 16;
    char *d = to;

    while (from < end) {
	*d++ = hexdigits[(*from >> 4)];
	*d++ = hexdigits[(*from & 0x0F)];
	from++;
    }
    *d = '\0';
    return to;
}

static char* base64_16(const unsigned char* from, char* to)
{
    static const char base64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *end = from + 16;
    unsigned char c1, c2, c3;
    char *d = to;

    while (1) {
	c1 = *from++;
	*d++ = base64[c1>>2];
	if (from == end) {
	    *d++ = base64[(c1 & 0x3) << 4];
	    break;
	}
	c2 = *from++;
	c3 = *from++;
	*d++ = base64[((c1 & 0x3) << 4) | ((c2 & 0xF0) >> 4)];
	*d++ = base64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
	*d++ = base64[c3 & 0x3F];
    }
    *d = '\0';
    return to;
}

/* Formats */
#define F_BIN 0
#define F_HEX 1
#define F_B64 2

static SV* make_mortal_sv(pTHX_ const unsigned char *src, int type)
{
    STRLEN len;
    char result[33];
    char *ret;
    
    switch (type) {
    case F_BIN:
	ret = (char*)src;
	len = 16;
	break;
    case F_HEX:
	ret = hex_16(src, result);
	len = 32;
	break;
    case F_B64:
	ret = base64_16(src, result);
	len = 22;
	break;
    default:
	croak("Bad conversion type (%d)", type);
	break;
    }
    return sv_2mortal(newSVpv(ret,len));
}


/********************************************************************/

typedef PerlIO* InputStream;

MODULE = Digest::MD5		PACKAGE = Digest::MD5

PROTOTYPES: DISABLE

void
new(xclass)
	SV* xclass
    PREINIT:
	MD5_CTX* context;
    PPCODE:
	if (!SvROK(xclass)) {
	    STRLEN my_na;
	    const char *sclass = SvPV(xclass, my_na);
	    New(55, context, 1, MD5_CTX);
	    ST(0) = sv_2mortal(new_md5_ctx(aTHX_ context, sclass));
	} else {
	    context = get_md5_ctx(aTHX_ xclass);
	}
	MD5Init(context);
	XSRETURN(1);

void
clone(self)
	SV* self
    PREINIT:
	MD5_CTX* cont = get_md5_ctx(aTHX_ self);
	const char *myname = sv_reftype(SvRV(self),TRUE);
	MD5_CTX* context;
    PPCODE:
	New(55, context, 1, MD5_CTX);
	ST(0) = sv_2mortal(new_md5_ctx(aTHX_ context, myname));
	memcpy(context,cont,sizeof(MD5_CTX));
	XSRETURN(1);

void
DESTROY(context)
	MD5_CTX* context
    CODE:
        Safefree(context);

void
add(self, ...)
	SV* self
    PREINIT:
	MD5_CTX* context = get_md5_ctx(aTHX_ self);
	int i;
	unsigned char *data;
	STRLEN len;
    PPCODE:
	for (i = 1; i < items; i++) {
            U32 had_utf8 = SvUTF8(ST(i));
	    data = (unsigned char *)(SvPVbyte(ST(i), len));
	    MD5Update(context, data, len);
	    if (had_utf8) sv_utf8_upgrade(ST(i));
	}
	XSRETURN(1);  /* self */

void
addfile(self, fh)
	SV* self
	InputStream fh
    PREINIT:
	MD5_CTX* context = get_md5_ctx(aTHX_ self);
	STRLEN fill = (context->count >> 3) & (MD5_BLOCK_LENGTH - 1);
#ifdef USE_HEAP_INSTEAD_OF_STACK
	unsigned char* buffer;
#else
	unsigned char buffer[4096];
#endif
	int  n;
    CODE:
	if (fh) {
#ifdef USE_HEAP_INSTEAD_OF_STACK
	    New(0, buffer, 4096, unsigned char);
	    assert(buffer);
#endif
            if (fill) {
	        /* The MD5Update() function is faster if it can work with
	         * complete blocks.  This will fill up any buffered block
	         * first.
	         */
	        STRLEN missing = 64 - fill;
	        if ( (n = PerlIO_read(fh, buffer, missing)) > 0)
	 	    MD5Update(context, buffer, n);
	        else
		    XSRETURN(1);  /* self */
	    }

	    /* Process blocks until EOF or error */
            while ( (n = PerlIO_read(fh, buffer, sizeof(buffer))) > 0) {
	        MD5Update(context, buffer, n);
	    }
#ifdef USE_HEAP_INSTEAD_OF_STACK
	    Safefree(buffer);
#endif
	    if (PerlIO_error(fh)) {
		croak("Reading from filehandle failed");
	    }
	}
	else {
	    croak("No filehandle passed");
	}
	XSRETURN(1);  /* self */

void
digest(context)
	MD5_CTX* context
    ALIAS:
	Digest::MD5::digest    = F_BIN
	Digest::MD5::hexdigest = F_HEX
	Digest::MD5::b64digest = F_B64
    PREINIT:
	unsigned char digeststr[16];
    PPCODE:
        MD5Final(digeststr, context);
	MD5Init(context);  /* In case it is reused */
        ST(0) = make_mortal_sv(aTHX_ digeststr, ix);
        XSRETURN(1);

void
md5(...)
    ALIAS:
	Digest::MD5::md5        = F_BIN
	Digest::MD5::md5_hex    = F_HEX
	Digest::MD5::md5_base64 = F_B64
    PREINIT:
	MD5_CTX ctx;
	int i;
	unsigned char *data;
        STRLEN len;
	unsigned char digeststr[16];
    PPCODE:
	MD5Init(&ctx);

	if (PL_dowarn & G_WARN_ON) {
            const char *msg = 0;
	    if (items == 1) {
		if (SvROK(ST(0))) {
                    SV* sv = SvRV(ST(0));
		    if (SvOBJECT(sv) && strEQ(HvNAME(SvSTASH(sv)), "Digest::MD5"))
		        msg = "probably called as method";
		    else
			msg = "called with reference argument";
		}
	    }
	    else if (items > 1) {
		data = (unsigned char *)SvPV(ST(0), len);
		if (len == 11 && memEQ("Digest::MD5", data, 11)) {
		    msg = "probably called as class method";
		}
		else if (SvROK(ST(0))) {
		    SV* sv = SvRV(ST(0));
		    if (SvOBJECT(sv) && strEQ(HvNAME(SvSTASH(sv)), "Digest::MD5"))
		        msg = "probably called as method";
		}
	    }
	    if (msg) {
	        const char *f = (ix == F_BIN) ? "md5" :
		                (ix == F_HEX) ? "md5_hex" : "md5_base64";
	        warn("&Digest::MD5::%s function %s", f, msg);
	    }
	}

	for (i = 0; i < items; i++) {
            U32 had_utf8 = SvUTF8(ST(i));
	    data = (unsigned char *)(SvPVbyte(ST(i), len));
	    MD5Update(&ctx, data, len);
	    if (had_utf8) sv_utf8_upgrade(ST(i));
	}
	MD5Final(digeststr, &ctx);
        ST(0) = make_mortal_sv(aTHX_ digeststr, ix);
        XSRETURN(1);
