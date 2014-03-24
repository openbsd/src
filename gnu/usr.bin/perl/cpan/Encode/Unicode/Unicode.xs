/*
 $Id: Unicode.xs,v 2.9 2012/08/05 23:08:49 dankogai Exp $
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#define U8 U8
#include "../Encode/encode.h"

#define FBCHAR			0xFFFd
#define BOM_BE			0xFeFF
#define BOM16LE			0xFFFe
#define BOM32LE			0xFFFe0000
#define issurrogate(x)		(0xD800 <= (x)  && (x) <= 0xDFFF )
#define isHiSurrogate(x)	(0xD800 <= (x)  && (x) <  0xDC00 )
#define isLoSurrogate(x)	(0xDC00 <= (x)  && (x) <= 0xDFFF )
#define invalid_ucs2(x)         ( issurrogate(x) || 0xFFFF < (x) )

/* For pre-5.14 source compatibility */
#ifndef UNICODE_WARN_ILLEGAL_INTERCHANGE
#   define UNICODE_WARN_ILLEGAL_INTERCHANGE 0
#   define UTF8_DISALLOW_SURROGATE 0
#   define UTF8_WARN_SURROGATE 0
#   define UTF8_DISALLOW_FE_FF 0
#   define UTF8_WARN_FE_FF 0
#   define UTF8_WARN_NONCHAR 0
#endif

#define PERLIO_BUFSIZ 1024 /* XXX value comes from PerlIOEncode_get_base */

/* Avoid wasting too much space in the result buffer */
/* static void */
/* shrink_buffer(SV *result) */
/* { */
/*     if (SvLEN(result) > 42 + SvCUR(result)) { */
/* 	char *buf; */
/* 	STRLEN len = 1 + SvCUR(result); /\* include the NUL byte *\/ */
/* 	New(0, buf, len, char); */
/* 	Copy(SvPVX(result), buf, len, char); */
/* 	Safefree(SvPVX(result)); */
/* 	SvPV_set(result, buf); */
/* 	SvLEN_set(result, len); */
/*     } */
/* } */

#define shrink_buffer(result) { \
    if (SvLEN(result) > 42 + SvCUR(result)) { \
	char *newpv; \
	STRLEN newlen = 1 + SvCUR(result); /* include the NUL byte */ \
	New(0, newpv, newlen, char); \
	Copy(SvPVX(result), newpv, newlen, char); \
	Safefree(SvPVX(result)); \
	SvPV_set(result, newpv); \
	SvLEN_set(result, newlen); \
    } \
}

static UV
enc_unpack(pTHX_ U8 **sp, U8 *e, STRLEN size, U8 endian)
{
    U8 *s = *sp;
    UV v = 0;
    if (s+size > e) {
	croak("Partial character %c",(char) endian);
    }
    switch(endian) {
    case 'N':
	v = *s++;
	v = (v << 8) | *s++;
    case 'n':
	v = (v << 8) | *s++;
	v = (v << 8) | *s++;
	break;
    case 'V':
    case 'v':
	v |= *s++;
	v |= (*s++ << 8);
	if (endian == 'v')
	    break;
	v |= (*s++ << 16);
	v |= (*s++ << 24);
	break;
    default:
	croak("Unknown endian %c",(char) endian);
	break;
    }
    *sp = s;
    return v;
}

void
enc_pack(pTHX_ SV *result, STRLEN size, U8 endian, UV value)
{
    U8 *d = (U8 *) SvPV_nolen(result);

    switch(endian) {
    case 'v':
    case 'V':
	d += SvCUR(result);
	SvCUR_set(result,SvCUR(result)+size);
	while (size--) {
	    *d++ = (U8)(value & 0xFF);
	    value >>= 8;
	}
	break;
    case 'n':
    case 'N':
	SvCUR_set(result,SvCUR(result)+size);
	d += SvCUR(result);
	while (size--) {
	    *--d = (U8)(value & 0xFF);
	    value >>= 8;
	}
	break;
    default:
	croak("Unknown endian %c",(char) endian);
	break;
    }
}

MODULE = Encode::Unicode PACKAGE = Encode::Unicode

PROTOTYPES: DISABLE

#define attr(k, l)  (hv_exists((HV *)SvRV(obj),k,l) ? \
    *hv_fetch((HV *)SvRV(obj),k,l,0) : &PL_sv_undef)

void
decode_xs(obj, str, check = 0)
SV *	obj
SV *	str
IV	check
CODE:
{
    U8 endian    = *((U8 *)SvPV_nolen(attr("endian", 6)));
    int size     = SvIV(attr("size", 4));
    int ucs2     = -1; /* only needed in the event of surrogate pairs */
    SV *result   = newSVpvn("",0);
    STRLEN usize = (size > 0 ? size : 1); /* protect against rogue size<=0 */
    STRLEN ulen;
    STRLEN resultbuflen;
    U8 *resultbuf;
    U8 *s = (U8 *)SvPVbyte(str,ulen);
    U8 *e = (U8 *)SvEND(str);
    /* Optimise for the common case of being called from PerlIOEncode_fill()
       with a standard length buffer. In this case the result SV's buffer is
       only used temporarily, so we can afford to allocate the maximum needed
       and not care about unused space. */
    const bool temp_result = (ulen == PERLIO_BUFSIZ);

    ST(0) = sv_2mortal(result);
    SvUTF8_on(result);

    if (!endian && s+size <= e) {
	UV bom;
	endian = (size == 4) ? 'N' : 'n';
	bom = enc_unpack(aTHX_ &s,e,size,endian);
	if (bom != BOM_BE) {
	    if (bom == BOM16LE) {
		endian = 'v';
	    }
	    else if (bom == BOM32LE) {
		endian = 'V';
	    }
	    else {
		croak("%"SVf":Unrecognised BOM %"UVxf,
		      *hv_fetch((HV *)SvRV(obj),"Name",4,0),
		      bom);
	    }
	}
#if 1
	/* Update endian for next sequence */
	if (SvTRUE(attr("renewed", 7))) {
	    hv_store((HV *)SvRV(obj),"endian",6,newSVpv((char *)&endian,1),0);
	}
#endif
    }

    if (temp_result) {
	resultbuflen = 1 + ulen/usize * UTF8_MAXLEN;
    } else {
	/* Preallocate the buffer to the minimum possible space required. */
	resultbuflen = ulen/usize + UTF8_MAXLEN + 1;
    }
    resultbuf = (U8 *) SvGROW(result, resultbuflen);

    while (s < e && s+size <= e) {
	UV ord = enc_unpack(aTHX_ &s,e,size,endian);
	U8 *d;
	if (issurrogate(ord)) {
	    if (ucs2 == -1) {
		ucs2 = SvTRUE(attr("ucs2", 4));
	    }
	    if (ucs2 || size == 4) {
		if (check) {
		    croak("%"SVf":no surrogates allowed %"UVxf,
			  *hv_fetch((HV *)SvRV(obj),"Name",4,0),
			  ord);
		}
		ord = FBCHAR;
	    }
	    else {
		UV lo;
		if (!isHiSurrogate(ord)) {
		    if (check) {
			croak("%"SVf":Malformed HI surrogate %"UVxf,
			      *hv_fetch((HV *)SvRV(obj),"Name",4,0),
			      ord);
		    }
		    else {
			ord = FBCHAR;
		    }
		}
		else if (s+size > e) {
		    if (check) {
		        if (check & ENCODE_STOP_AT_PARTIAL) {
		             s -= size;
		             break;
		        }
		        else {
		             croak("%"SVf":Malformed HI surrogate %"UVxf,
				   *hv_fetch((HV *)SvRV(obj),"Name",4,0),
				   ord);
		        }
		    }
		    else {
		        ord = FBCHAR;
		    }
		}
		else {
		    lo = enc_unpack(aTHX_ &s,e,size,endian);
		    if (!isLoSurrogate(lo)) {
			if (check) {
			    croak("%"SVf":Malformed LO surrogate %"UVxf,
				  *hv_fetch((HV *)SvRV(obj),"Name",4,0),
				  ord);
			}
			else {
			    s -= size;
			    ord = FBCHAR;
			}
		    }
		    else {
			ord = 0x10000 + ((ord - 0xD800) << 10) + (lo - 0xDC00);
		    }
		}
	    }
	}

	if ((ord & 0xFFFE) == 0xFFFE || (ord >= 0xFDD0 && ord <= 0xFDEF)) {
	    if (check) {
		croak("%"SVf":Unicode character %"UVxf" is illegal",
		      *hv_fetch((HV *)SvRV(obj),"Name",4,0),
		      ord);
	    } else {
		ord = FBCHAR;
	    }
	}

	if (resultbuflen < SvCUR(result) + UTF8_MAXLEN + 1) {
	    /* Do not allocate >8Mb more than the minimum needed.
	       This prevents allocating too much in the rogue case of a large
	       input consisting initially of long sequence uft8-byte unicode
	       chars followed by single utf8-byte chars. */
            /* +1 
               fixes  Unicode.xs!decode_xs n-byte heap-overflow
              */
	    STRLEN remaining = (e - s)/usize + 1; /* +1 to avoid the leak */
	    STRLEN max_alloc = remaining + (8*1024*1024);
	    STRLEN est_alloc = remaining * UTF8_MAXLEN;
	    STRLEN newlen = SvLEN(result) + /* min(max_alloc, est_alloc) */
		(est_alloc > max_alloc ? max_alloc : est_alloc);
	    resultbuf = (U8 *) SvGROW(result, newlen);
	    resultbuflen = SvLEN(result);
	}

	d = uvuni_to_utf8_flags(resultbuf+SvCUR(result), ord,
                                            UNICODE_WARN_ILLEGAL_INTERCHANGE);
	SvCUR_set(result, d - (U8 *)SvPVX(result));
    }

    if (s < e) {
	/* unlikely to happen because it's fixed-length -- dankogai */
	if (check & ENCODE_WARN_ON_ERR) {
	    Perl_warner(aTHX_ packWARN(WARN_UTF8),"%"SVf":Partial character",
			*hv_fetch((HV *)SvRV(obj),"Name",4,0));
	}
    }
    if (check && !(check & ENCODE_LEAVE_SRC)) {
	if (s < e) {
	    Move(s,SvPVX(str),e-s,U8);
	    SvCUR_set(str,(e-s));
	}
	else {
	    SvCUR_set(str,0);
	}
	*SvEND(str) = '\0';
    }

    if (!temp_result)
	shrink_buffer(result);

    XSRETURN(1);
}

void
encode_xs(obj, utf8, check = 0)
SV *	obj
SV *	utf8
IV	check
CODE:
{
    U8 endian = *((U8 *)SvPV_nolen(attr("endian", 6)));
    const int size = SvIV(attr("size", 4));
    int ucs2 = -1; /* only needed if there is invalid_ucs2 input */
    const STRLEN usize = (size > 0 ? size : 1);
    SV *result = newSVpvn("", 0);
    STRLEN ulen;
    U8 *s = (U8 *) SvPVutf8(utf8, ulen);
    const U8 *e = (U8 *) SvEND(utf8);
    /* Optimise for the common case of being called from PerlIOEncode_flush()
       with a standard length buffer. In this case the result SV's buffer is
       only used temporarily, so we can afford to allocate the maximum needed
       and not care about unused space. */
    const bool temp_result = (ulen == PERLIO_BUFSIZ);

    ST(0) = sv_2mortal(result);

    /* Preallocate the result buffer to the maximum possible size.
       ie. assume each UTF8 byte is 1 character.
       Then shrink the result's buffer if necesary at the end. */
    SvGROW(result, ((ulen+1) * usize));

    if (!endian) {
	endian = (size == 4) ? 'N' : 'n';
	enc_pack(aTHX_ result,size,endian,BOM_BE);
#if 1
	/* Update endian for next sequence */
	if (SvTRUE(attr("renewed", 7))) {
	    hv_store((HV *)SvRV(obj),"endian",6,newSVpv((char *)&endian,1),0);
	}
#endif
    }
    while (s < e && s+UTF8SKIP(s) <= e) {
	STRLEN len;
	UV ord = utf8n_to_uvuni(s, e-s, &len, (UTF8_DISALLOW_SURROGATE
                                               |UTF8_WARN_SURROGATE
                                               |UTF8_DISALLOW_FE_FF
                                               |UTF8_WARN_FE_FF
                                               |UTF8_WARN_NONCHAR));
	s += len;
	if (size != 4 && invalid_ucs2(ord)) {
	    if (!issurrogate(ord)) {
		if (ucs2 == -1) {
		    ucs2 = SvTRUE(attr("ucs2", 4));
		}
		if (ucs2 || ord > 0x10FFFF) {
		    if (check) {
			croak("%"SVf":code point \"\\x{%"UVxf"}\" too high",
				  *hv_fetch((HV *)SvRV(obj),"Name",4,0),ord);
		    }
		    enc_pack(aTHX_ result,size,endian,FBCHAR);
		} else {
		    UV hi = ((ord - 0x10000) >> 10)   + 0xD800;
		    UV lo = ((ord - 0x10000) & 0x3FF) + 0xDC00;
		    enc_pack(aTHX_ result,size,endian,hi);
		    enc_pack(aTHX_ result,size,endian,lo);
		}
	    }
	    else {
		/* not supposed to happen */
		enc_pack(aTHX_ result,size,endian,FBCHAR);
	    }
	}
	else {
	    enc_pack(aTHX_ result,size,endian,ord);
	}
    }
    if (s < e) {
	/* UTF-8 partial char happens often on PerlIO.
	   Since this is okay and normal, we do not warn.
	   But this is critical when you choose to LEAVE_SRC
	   in which case we die */
	if (check & (ENCODE_DIE_ON_ERR|ENCODE_LEAVE_SRC)) {
	    Perl_croak(aTHX_ "%"SVf":partial character is not allowed "
		       "when CHECK = 0x%" UVuf,
		       *hv_fetch((HV *)SvRV(obj),"Name",4,0), check);
	}
    }
    if (check && !(check & ENCODE_LEAVE_SRC)) {
	if (s < e) {
	    Move(s,SvPVX(utf8),e-s,U8);
	    SvCUR_set(utf8,(e-s));
	}
	else {
	    SvCUR_set(utf8,0);
	}
	*SvEND(utf8) = '\0';
    }

    if (!temp_result)
	shrink_buffer(result);

    SvSETMAGIC(utf8);

    XSRETURN(1);
}
