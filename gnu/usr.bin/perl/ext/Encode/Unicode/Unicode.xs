/*
 $Id: Unicode.xs,v 1.5 2002/05/20 15:25:44 dankogai Exp $
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

static UV
enc_unpack(pTHX_ U8 **sp,U8 *e,STRLEN size,U8 endian)
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
enc_pack(pTHX_ SV *result,STRLEN size,U8 endian,UV value)
{
    U8 *d = (U8 *)SvGROW(result,SvCUR(result)+size);
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

void
decode_xs(obj, str, check = 0)
SV *	obj
SV *	str
IV	check
CODE:
{
    int size    = SvIV(*hv_fetch((HV *)SvRV(obj),"size",4,0));
    U8 endian   = *((U8 *)SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"endian",6,0)));
    int ucs2    = SvTRUE(*hv_fetch((HV *)SvRV(obj),"ucs2",4,0));
    SV *result = newSVpvn("",0);
    STRLEN ulen;
    U8 *s = (U8 *)SvPVbyte(str,ulen);
    U8 *e = (U8 *)SvEND(str);
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
		croak("%s:Unregognised BOM %"UVxf,
                      SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)),
		      bom);
	    }
	}
#if 0
	/* Update endian for this sequence */
	hv_store((HV *)SvRV(obj),"endian",6,newSVpv((char *)&endian,1),0);
#endif
    }
    while (s < e && s+size <= e) {
	UV ord = enc_unpack(aTHX_ &s,e,size,endian);
	U8 *d;
	if (size != 4 && invalid_ucs2(ord)) {
	    if (ucs2) {
		if (check) {
		    croak("%s:no surrogates allowed %"UVxf,
			  SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)),
			  ord);
		}
		if (s+size <= e) {
                    /* skip the next one as well */
		    enc_unpack(aTHX_ &s,e,size,endian);
		}
		ord = FBCHAR;
	    }
	    else {
		UV lo;
		if (!isHiSurrogate(ord)) {
		    croak("%s:Malformed HI surrogate %"UVxf,
			  SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)),
			  ord);
		}
		if (s+size > e) {
		    /* Partial character */
		    s -= size;   /* back up to 1st half */
		    break;       /* And exit loop */
		}
		lo = enc_unpack(aTHX_ &s,e,size,endian);
		if (!isLoSurrogate(lo)){
		    croak("%s:Malformed LO surrogate %"UVxf,
			  SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)),
			  ord);
		}
		ord = 0x10000 + ((ord - 0xD800) << 10) + (lo - 0xDC00);
	    }
	}
	d = (U8 *) SvGROW(result,SvCUR(result)+UTF8_MAXLEN+1);
	d = uvuni_to_utf8_flags(d+SvCUR(result), ord, 0);
	SvCUR_set(result,d - (U8 *)SvPVX(result));
    }
    if (s < e) {
	    Perl_warner(aTHX_ packWARN(WARN_UTF8),"%s:Partial character",
			SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)));
    }
    if (check && !(check & ENCODE_LEAVE_SRC)){
	if (s < e) {
	    Move(s,SvPVX(str),e-s,U8);
	    SvCUR_set(str,(e-s));
	}
	else {
	    SvCUR_set(str,0);
	}
	*SvEND(str) = '\0';
    }
    XSRETURN(1);
}

void
encode_xs(obj, utf8, check = 0)
SV *	obj
SV *	utf8
IV	check
CODE:
{
    int size   = SvIV(*hv_fetch((HV *)SvRV(obj),"size",4,0));
    U8 endian = *((U8 *)SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"endian",6,0)));
    int ucs2   = SvTRUE(*hv_fetch((HV *)SvRV(obj),"ucs2",4,0));
    SV *result = newSVpvn("",0);
    STRLEN ulen;
    U8 *s = (U8 *)SvPVutf8(utf8,ulen);
    U8 *e = (U8 *)SvEND(utf8);
    ST(0) = sv_2mortal(result);
    if (!endian) {
	endian = (size == 4) ? 'N' : 'n';
	enc_pack(aTHX_ result,size,endian,BOM_BE);
#if 0
	/* Update endian for this sequence */
	hv_store((HV *)SvRV(obj),"endian",6,newSVpv((char *)&endian,1),0);
#endif
    }
    while (s < e && s+UTF8SKIP(s) <= e) {
	STRLEN len;
	UV ord = utf8n_to_uvuni(s, e-s, &len, 0);
        s += len;
	if (size != 4 && invalid_ucs2(ord)) {
	    if (!issurrogate(ord)){
		if (ucs2) {
		    if (check) {
			croak("%s:code point \"\\x{%"UVxf"}\" too high",
			      SvPV_nolen(
				  *hv_fetch((HV *)SvRV(obj),"Name",4,0))
			      ,ord);
		    }
		    enc_pack(aTHX_ result,size,endian,FBCHAR);
		}else{
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
	Perl_warner(aTHX_ packWARN(WARN_UTF8),"%s:Partial character",
		    SvPV_nolen(*hv_fetch((HV *)SvRV(obj),"Name",4,0)));
    }
    if (check && !(check & ENCODE_LEAVE_SRC)){
	if (s < e) {
	    Move(s,SvPVX(utf8),e-s,U8);
	    SvCUR_set(utf8,(e-s));
	}
	else {
	    SvCUR_set(utf8,0);
	}
	*SvEND(utf8) = '\0';
    }
    XSRETURN(1);
}

