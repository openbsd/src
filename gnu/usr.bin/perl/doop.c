/*    doop.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2004, 2005, 2006, 2007, 2008, 2009 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *  'So that was the job I felt I had to do when I started,' thought Sam.
 *
 *     [p.934 of _The Lord of the Rings_, VI/iii: "Mount Doom"]
 */

/* This file contains some common functions needed to carry out certain
 * ops. For example, both pp_sprintf() and pp_prtf() call the function
 * do_sprintf() found in this file.
 */

#include "EXTERN.h"
#define PERL_IN_DOOP_C
#include "perl.h"

#ifndef PERL_MICRO
#include <signal.h>
#endif

STATIC I32
S_do_trans_simple(pTHX_ SV * const sv)
{
    I32 matches = 0;
    STRLEN len;
    U8 *s = (U8*)SvPV_nomg(sv,len);
    U8 * const send = s+len;
    const short * const tbl = (short*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_SIMPLE;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_simple line %d",__LINE__);

    /* First, take care of non-UTF-8 input strings, because they're easy */
    if (!SvUTF8(sv)) {
	while (s < send) {
	    const I32 ch = tbl[*s];
	    if (ch >= 0) {
		matches++;
		*s = (U8)ch;
	    }
	    s++;
	}
	SvSETMAGIC(sv);
    }
    else {
	const I32 grows = PL_op->op_private & OPpTRANS_GROWS;
	U8 *d;
	U8 *dstart;

	/* Allow for expansion: $_="a".chr(400); tr/a/\xFE/, FE needs encoding */
	if (grows)
	    Newx(d, len*2+1, U8);
	else
	    d = s;
	dstart = d;
	while (s < send) {
	    STRLEN ulen;
	    I32 ch;

	    /* Need to check this, otherwise 128..255 won't match */
	    const UV c = utf8n_to_uvchr(s, send - s, &ulen, UTF8_ALLOW_DEFAULT);
	    if (c < 0x100 && (ch = tbl[c]) >= 0) {
		matches++;
		d = uvchr_to_utf8(d, ch);
		s += ulen;
	    }
	    else { /* No match -> copy */
		Move(s, d, ulen, U8);
		d += ulen;
		s += ulen;
	    }
	}
	if (grows) {
	    sv_setpvn(sv, (char*)dstart, d - dstart);
	    Safefree(dstart);
	}
	else {
	    *d = '\0';
	    SvCUR_set(sv, d - dstart);
	}
	SvUTF8_on(sv);
	SvSETMAGIC(sv);
    }
    return matches;
}

STATIC I32
S_do_trans_count(pTHX_ SV * const sv)
{
    STRLEN len;
    const U8 *s = (const U8*)SvPV_nomg_const(sv, len);
    const U8 * const send = s + len;
    I32 matches = 0;
    const short * const tbl = (short*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_COUNT;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_count line %d",__LINE__);

    if (!SvUTF8(sv)) {
	while (s < send) {
            if (tbl[*s++] >= 0)
                matches++;
	}
    }
    else {
	const I32 complement = PL_op->op_private & OPpTRANS_COMPLEMENT;
	while (s < send) {
	    STRLEN ulen;
	    const UV c = utf8n_to_uvchr(s, send - s, &ulen, UTF8_ALLOW_DEFAULT);
	    if (c < 0x100) {
		if (tbl[c] >= 0)
		    matches++;
	    } else if (complement)
		matches++;
	    s += ulen;
	}
    }

    return matches;
}

STATIC I32
S_do_trans_complex(pTHX_ SV * const sv)
{
    STRLEN len;
    U8 *s = (U8*)SvPV_nomg(sv, len);
    U8 * const send = s+len;
    I32 matches = 0;
    const short * const tbl = (short*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_COMPLEX;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_complex line %d",__LINE__);

    if (!SvUTF8(sv)) {
	U8 *d = s;
	U8 * const dstart = d;

	if (PL_op->op_private & OPpTRANS_SQUASH) {
	    const U8* p = send;
	    while (s < send) {
		const I32 ch = tbl[*s];
		if (ch >= 0) {
		    *d = (U8)ch;
		    matches++;
		    if (p != d - 1 || *p != *d)
			p = d++;
		}
		else if (ch == -1)	/* -1 is unmapped character */
		    *d++ = *s;	
		else if (ch == -2)	/* -2 is delete character */
		    matches++;
		s++;
	    }
	}
	else {
	    while (s < send) {
		const I32 ch = tbl[*s];
		if (ch >= 0) {
		    matches++;
		    *d++ = (U8)ch;
		}
		else if (ch == -1)	/* -1 is unmapped character */
		    *d++ = *s;
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s++;
	    }
	}
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    else { /* is utf8 */
	const I32 complement = PL_op->op_private & OPpTRANS_COMPLEMENT;
	const I32 grows = PL_op->op_private & OPpTRANS_GROWS;
	const I32 del = PL_op->op_private & OPpTRANS_DELETE;
	U8 *d;
	U8 *dstart;
	STRLEN rlen = 0;

	if (grows)
	    Newx(d, len*2+1, U8);
	else
	    d = s;
	dstart = d;
	if (complement && !del)
	    rlen = tbl[0x100];

	if (PL_op->op_private & OPpTRANS_SQUASH) {
	    UV pch = 0xfeedface;
	    while (s < send) {
		STRLEN len;
		const UV comp = utf8n_to_uvchr(s, send - s, &len,
					       UTF8_ALLOW_DEFAULT);
		I32 ch;

		if (comp > 0xff) {
		    if (!complement) {
			Move(s, d, len, U8);
			d += len;
		    }
		    else {
			matches++;
			if (!del) {
			    ch = (rlen == 0) ? (I32)comp :
				(comp - 0x100 < rlen) ?
				tbl[comp+1] : tbl[0x100+rlen];
			    if ((UV)ch != pch) {
				d = uvchr_to_utf8(d, ch);
				pch = (UV)ch;
			    }
			    s += len;
			    continue;
			}
		    }
		}
		else if ((ch = tbl[comp]) >= 0) {
		    matches++;
		    if ((UV)ch != pch) {
		        d = uvchr_to_utf8(d, ch);
		        pch = (UV)ch;
		    }
		    s += len;
		    continue;
		}
		else if (ch == -1) {	/* -1 is unmapped character */
		    Move(s, d, len, U8);
		    d += len;
		}
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s += len;
		pch = 0xfeedface;
	    }
	}
	else {
	    while (s < send) {
		STRLEN len;
		const UV comp = utf8n_to_uvchr(s, send - s, &len,
					       UTF8_ALLOW_DEFAULT);
		I32 ch;
		if (comp > 0xff) {
		    if (!complement) {
			Move(s, d, len, U8);
			d += len;
		    }
		    else {
			matches++;
			if (!del) {
			    if (comp - 0x100 < rlen)
				d = uvchr_to_utf8(d, tbl[comp+1]);
			    else
				d = uvchr_to_utf8(d, tbl[0x100+rlen]);
			}
		    }
		}
		else if ((ch = tbl[comp]) >= 0) {
		    d = uvchr_to_utf8(d, ch);
		    matches++;
		}
		else if (ch == -1) {	/* -1 is unmapped character */
		    Move(s, d, len, U8);
		    d += len;
		}
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s += len;
	    }
	}
	if (grows) {
	    sv_setpvn(sv, (char*)dstart, d - dstart);
	    Safefree(dstart);
	}
	else {
	    *d = '\0';
	    SvCUR_set(sv, d - dstart);
	}
	SvUTF8_on(sv);
    }
    SvSETMAGIC(sv);
    return matches;
}

STATIC I32
S_do_trans_simple_utf8(pTHX_ SV * const sv)
{
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *start;
    U8 *dstart, *dend;
    I32 matches = 0;
    const I32 grows = PL_op->op_private & OPpTRANS_GROWS;
    STRLEN len;
    SV* const  rv =
#ifdef USE_ITHREADS
		    PAD_SVl(cPADOP->op_padix);
#else
		    MUTABLE_SV(cSVOP->op_sv);
#endif
    HV* const  hv = MUTABLE_HV(SvRV(rv));
    SV* const * svp = hv_fetchs(hv, "NONE", FALSE);
    const UV none = svp ? SvUV(*svp) : 0x7fffffff;
    const UV extra = none + 1;
    UV final = 0;
    U8 hibit = 0;

    PERL_ARGS_ASSERT_DO_TRANS_SIMPLE_UTF8;

    s = (U8*)SvPV_nomg(sv, len);
    if (!SvUTF8(sv)) {
	const U8 *t = s;
	const U8 * const e = s + len;
	while (t < e) {
	    const U8 ch = *t++;
	    hibit = !NATIVE_BYTE_IS_INVARIANT(ch);
	    if (hibit) {
		s = bytes_to_utf8(s, &len);
		break;
	    }
	}
    }
    send = s + len;
    start = s;

    svp = hv_fetchs(hv, "FINAL", FALSE);
    if (svp)
	final = SvUV(*svp);

    if (grows) {
	/* d needs to be bigger than s, in case e.g. upgrading is required */
	Newx(d, len * 3 + UTF8_MAXBYTES, U8);
	dend = d + len * 3;
	dstart = d;
    }
    else {
	dstart = d = s;
	dend = d + len;
    }

    while (s < send) {
	const UV uv = swash_fetch(rv, s, TRUE);
	if (uv < none) {
	    s += UTF8SKIP(s);
	    matches++;
	    d = uvchr_to_utf8(d, uv);
	}
	else if (uv == none) {
	    const int i = UTF8SKIP(s);
	    Move(s, d, i, U8);
	    d += i;
	    s += i;
	}
	else if (uv == extra) {
	    s += UTF8SKIP(s);
	    matches++;
	    d = uvchr_to_utf8(d, final);
	}
	else
	    s += UTF8SKIP(s);

	if (d > dend) {
	    const STRLEN clen = d - dstart;
	    const STRLEN nlen = dend - dstart + len + UTF8_MAXBYTES;
	    if (!grows)
		Perl_croak(aTHX_ "panic: do_trans_simple_utf8 line %d",__LINE__);
	    Renew(dstart, nlen + UTF8_MAXBYTES, U8);
	    d = dstart + clen;
	    dend = dstart + nlen;
	}
    }
    if (grows || hibit) {
	sv_setpvn(sv, (char*)dstart, d - dstart);
	Safefree(dstart);
	if (grows && hibit)
	    Safefree(start);
    }
    else {
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    SvSETMAGIC(sv);
    SvUTF8_on(sv);

    return matches;
}

STATIC I32
S_do_trans_count_utf8(pTHX_ SV * const sv)
{
    const U8 *s;
    const U8 *start = NULL;
    const U8 *send;
    I32 matches = 0;
    STRLEN len;
    SV* const  rv =
#ifdef USE_ITHREADS
		    PAD_SVl(cPADOP->op_padix);
#else
		    MUTABLE_SV(cSVOP->op_sv);
#endif
    HV* const hv = MUTABLE_HV(SvRV(rv));
    SV* const * const svp = hv_fetchs(hv, "NONE", FALSE);
    const UV none = svp ? SvUV(*svp) : 0x7fffffff;
    const UV extra = none + 1;
    U8 hibit = 0;

    PERL_ARGS_ASSERT_DO_TRANS_COUNT_UTF8;

    s = (const U8*)SvPV_nomg_const(sv, len);
    if (!SvUTF8(sv)) {
	const U8 *t = s;
	const U8 * const e = s + len;
	while (t < e) {
	    const U8 ch = *t++;
	    hibit = !NATIVE_BYTE_IS_INVARIANT(ch);
	    if (hibit) {
		start = s = bytes_to_utf8(s, &len);
		break;
	    }
	}
    }
    send = s + len;

    while (s < send) {
	const UV uv = swash_fetch(rv, s, TRUE);
	if (uv < none || uv == extra)
	    matches++;
	s += UTF8SKIP(s);
    }
    if (hibit)
        Safefree(start);

    return matches;
}

STATIC I32
S_do_trans_complex_utf8(pTHX_ SV * const sv)
{
    U8 *start, *send;
    U8 *d;
    I32 matches = 0;
    const I32 squash   = PL_op->op_private & OPpTRANS_SQUASH;
    const I32 del      = PL_op->op_private & OPpTRANS_DELETE;
    const I32 grows    = PL_op->op_private & OPpTRANS_GROWS;
    SV* const  rv =
#ifdef USE_ITHREADS
		    PAD_SVl(cPADOP->op_padix);
#else
		    MUTABLE_SV(cSVOP->op_sv);
#endif
    HV * const hv = MUTABLE_HV(SvRV(rv));
    SV * const *svp = hv_fetchs(hv, "NONE", FALSE);
    const UV none = svp ? SvUV(*svp) : 0x7fffffff;
    const UV extra = none + 1;
    UV final = 0;
    bool havefinal = FALSE;
    STRLEN len;
    U8 *dstart, *dend;
    U8 hibit = 0;
    U8 *s = (U8*)SvPV_nomg(sv, len);

    PERL_ARGS_ASSERT_DO_TRANS_COMPLEX_UTF8;

    if (!SvUTF8(sv)) {
	const U8 *t = s;
	const U8 * const e = s + len;
	while (t < e) {
	    const U8 ch = *t++;
	    hibit = !NATIVE_BYTE_IS_INVARIANT(ch);
	    if (hibit) {
		s = bytes_to_utf8(s, &len);
		break;
	    }
	}
    }
    send = s + len;
    start = s;

    svp = hv_fetchs(hv, "FINAL", FALSE);
    if (svp) {
	final = SvUV(*svp);
	havefinal = TRUE;
    }

    if (grows) {
	/* d needs to be bigger than s, in case e.g. upgrading is required */
	Newx(d, len * 3 + UTF8_MAXBYTES, U8);
	dend = d + len * 3;
	dstart = d;
    }
    else {
	dstart = d = s;
	dend = d + len;
    }

    if (squash) {
	UV puv = 0xfeedface;
	while (s < send) {
	    UV uv = swash_fetch(rv, s, TRUE);
	
	    if (d > dend) {
		const STRLEN clen = d - dstart;
		const STRLEN nlen = dend - dstart + len + UTF8_MAXBYTES;
		if (!grows)
		    Perl_croak(aTHX_ "panic: do_trans_complex_utf8 line %d",__LINE__);
		Renew(dstart, nlen + UTF8_MAXBYTES, U8);
		d = dstart + clen;
		dend = dstart + nlen;
	    }
	    if (uv < none) {
		matches++;
		s += UTF8SKIP(s);
		if (uv != puv) {
		    d = uvchr_to_utf8(d, uv);
		    puv = uv;
		}
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		const int i = UTF8SKIP(s);
		Move(s, d, i, U8);
		d += i;
		s += i;
		puv = 0xfeedface;
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		if (havefinal) {
		    s += UTF8SKIP(s);
		    if (puv != final) {
			d = uvchr_to_utf8(d, final);
			puv = final;
		    }
		}
		else {
		    STRLEN len;
		    uv = utf8n_to_uvchr(s, send - s, &len, UTF8_ALLOW_DEFAULT);
		    if (uv != puv) {
			Move(s, d, len, U8);
			d += len;
			puv = uv;
		    }
		    s += len;
		}
		continue;
	    }
	    matches++;			/* "none+1" is delete character */
	    s += UTF8SKIP(s);
	}
    }
    else {
	while (s < send) {
	    const UV uv = swash_fetch(rv, s, TRUE);
	    if (d > dend) {
	        const STRLEN clen = d - dstart;
		const STRLEN nlen = dend - dstart + len + UTF8_MAXBYTES;
		if (!grows)
		    Perl_croak(aTHX_ "panic: do_trans_complex_utf8 line %d",__LINE__);
		Renew(dstart, nlen + UTF8_MAXBYTES, U8);
		d = dstart + clen;
		dend = dstart + nlen;
	    }
	    if (uv < none) {
		matches++;
		s += UTF8SKIP(s);
		d = uvchr_to_utf8(d, uv);
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		const int i = UTF8SKIP(s);
		Move(s, d, i, U8);
		d += i;
		s += i;
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		s += UTF8SKIP(s);
		d = uvchr_to_utf8(d, final);
		continue;
	    }
	    matches++;			/* "none+1" is delete character */
	    s += UTF8SKIP(s);
	}
    }
    if (grows || hibit) {
	sv_setpvn(sv, (char*)dstart, d - dstart);
	Safefree(dstart);
	if (grows && hibit)
	    Safefree(start);
    }
    else {
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    SvUTF8_on(sv);
    SvSETMAGIC(sv);

    return matches;
}

I32
Perl_do_trans(pTHX_ SV *sv)
{
    STRLEN len;
    const I32 flags = PL_op->op_private;
    const I32 hasutf = flags & (OPpTRANS_FROM_UTF | OPpTRANS_TO_UTF);

    PERL_ARGS_ASSERT_DO_TRANS;

    if (SvREADONLY(sv) && !(flags & OPpTRANS_IDENTICAL)) {
        Perl_croak_no_modify();
    }
    (void)SvPV_const(sv, len);
    if (!len)
	return 0;
    if (!(flags & OPpTRANS_IDENTICAL)) {
	if (!SvPOKp(sv) || SvTHINKFIRST(sv))
	    (void)SvPV_force_nomg(sv, len);
	(void)SvPOK_only_UTF8(sv);
    }

    DEBUG_t( Perl_deb(aTHX_ "2.TBL\n"));

    /* If we use only OPpTRANS_IDENTICAL to bypass the READONLY check,
     * we must also rely on it to choose the readonly strategy.
     */
    if (flags & OPpTRANS_IDENTICAL) {
        return hasutf ? do_trans_count_utf8(sv) : do_trans_count(sv);
    } else if (flags & (OPpTRANS_SQUASH|OPpTRANS_DELETE|OPpTRANS_COMPLEMENT)) {
        return hasutf ? do_trans_complex_utf8(sv) : do_trans_complex(sv);
    } else {
        return hasutf ? do_trans_simple_utf8(sv) : do_trans_simple(sv);
    }
}

void
Perl_do_join(pTHX_ SV *sv, SV *delim, SV **mark, SV **sp)
{
    SV ** const oldmark = mark;
    I32 items = sp - mark;
    STRLEN len;
    STRLEN delimlen;
    const char * const delims = SvPV_const(delim, delimlen);

    PERL_ARGS_ASSERT_DO_JOIN;

    mark++;
    len = (items > 0 ? (delimlen * (items - 1) ) : 0);
    SvUPGRADE(sv, SVt_PV);
    if (SvLEN(sv) < len + items) {	/* current length is way too short */
	while (items-- > 0) {
	    if (*mark && !SvGAMAGIC(*mark) && SvOK(*mark)) {
		STRLEN tmplen;
		SvPV_const(*mark, tmplen);
		len += tmplen;
	    }
	    mark++;
	}
	SvGROW(sv, len + 1);		/* so try to pre-extend */

	mark = oldmark;
	items = sp - mark;
	++mark;
    }

    sv_setpvs(sv, "");
    /* sv_setpv retains old UTF8ness [perl #24846] */
    SvUTF8_off(sv);

    if (TAINTING_get && SvMAGICAL(sv))
	SvTAINTED_off(sv);

    if (items-- > 0) {
	if (*mark)
	    sv_catsv(sv, *mark);
	mark++;
    }

    if (delimlen) {
	const U32 delimflag = DO_UTF8(delim) ? SV_CATUTF8 : SV_CATBYTES;
	for (; items > 0; items--,mark++) {
	    STRLEN len;
	    const char *s;
	    sv_catpvn_flags(sv,delims,delimlen,delimflag);
	    s = SvPV_const(*mark,len);
	    sv_catpvn_flags(sv,s,len,
			    DO_UTF8(*mark) ? SV_CATUTF8 : SV_CATBYTES);
	}
    }
    else {
	for (; items > 0; items--,mark++)
	{
	    STRLEN len;
	    const char *s = SvPV_const(*mark,len);
	    sv_catpvn_flags(sv,s,len,
			    DO_UTF8(*mark) ? SV_CATUTF8 : SV_CATBYTES);
	}
    }
    SvSETMAGIC(sv);
}

void
Perl_do_sprintf(pTHX_ SV *sv, I32 len, SV **sarg)
{
    STRLEN patlen;
    const char * const pat = SvPV_const(*sarg, patlen);
    bool do_taint = FALSE;

    PERL_ARGS_ASSERT_DO_SPRINTF;

    if (SvTAINTED(*sarg))
	TAINT_PROPER(
		(PL_op && PL_op->op_type < OP_max)
		    ? (PL_op->op_type == OP_PRTF)
			? "printf"
			: PL_op_name[PL_op->op_type]
		    : "(unknown)"
	);
    SvUTF8_off(sv);
    if (DO_UTF8(*sarg))
        SvUTF8_on(sv);
    sv_vsetpvfn(sv, pat, patlen, NULL, sarg + 1, len - 1, &do_taint);
    SvSETMAGIC(sv);
    if (do_taint)
	SvTAINTED_on(sv);
}

/* currently converts input to bytes if possible, but doesn't sweat failure */
UV
Perl_do_vecget(pTHX_ SV *sv, SSize_t offset, int size)
{
    STRLEN srclen, len, uoffset, bitoffs = 0;
    const I32 svpv_flags = ((PL_op->op_flags & OPf_MOD || LVRET)
                                          ? SV_UNDEF_RETURNS_NULL : 0);
    unsigned char *s = (unsigned char *)
                            SvPV_flags(sv, srclen, (svpv_flags|SV_GMAGIC));
    UV retnum = 0;

    if (!s) {
      s = (unsigned char *)"";
    }
    
    PERL_ARGS_ASSERT_DO_VECGET;

    if (offset < 0)
	return 0;
    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    if (SvUTF8(sv)) {
	(void) Perl_sv_utf8_downgrade(aTHX_ sv, TRUE);
        /* PVX may have changed */
        s = (unsigned char *) SvPV_flags(sv, srclen, svpv_flags);
    }

    if (size < 8) {
	bitoffs = ((offset%8)*size)%8;
	uoffset = offset/(8/size);
    }
    else if (size > 8)
	uoffset = offset*(size/8);
    else
	uoffset = offset;

    len = uoffset + (bitoffs + size + 7)/8;	/* required number of bytes */
    if (len > srclen) {
	if (size <= 8)
	    retnum = 0;
	else {
	    if (size == 16) {
		if (uoffset >= srclen)
		    retnum = 0;
		else
		    retnum = (UV) s[uoffset] <<  8;
	    }
	    else if (size == 32) {
		if (uoffset >= srclen)
		    retnum = 0;
		else if (uoffset + 1 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 24);
		else if (uoffset + 2 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 24) +
			((UV) s[uoffset + 1] << 16);
		else
		    retnum =
			((UV) s[uoffset    ] << 24) +
			((UV) s[uoffset + 1] << 16) +
			(     s[uoffset + 2] <<  8);
	    }
#ifdef UV_IS_QUAD
	    else if (size == 64) {
		Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
			       "Bit vector size > 32 non-portable");
		if (uoffset >= srclen)
		    retnum = 0;
		else if (uoffset + 1 >= srclen)
		    retnum =
			(UV) s[uoffset     ] << 56;
		else if (uoffset + 2 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48);
		else if (uoffset + 3 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40);
		else if (uoffset + 4 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32);
		else if (uoffset + 5 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32) +
			((UV) s[uoffset + 4] << 24);
		else if (uoffset + 6 >= srclen)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32) +
			((UV) s[uoffset + 4] << 24) +
			((UV) s[uoffset + 5] << 16);
		else
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32) +
			((UV) s[uoffset + 4] << 24) +
			((UV) s[uoffset + 5] << 16) +
			((UV) s[uoffset + 6] <<  8);
	    }
#endif
	}
    }
    else if (size < 8)
	retnum = (s[uoffset] >> bitoffs) & ((1 << size) - 1);
    else {
	if (size == 8)
	    retnum = s[uoffset];
	else if (size == 16)
	    retnum =
		((UV) s[uoffset] <<      8) +
		      s[uoffset + 1];
	else if (size == 32)
	    retnum =
		((UV) s[uoffset    ] << 24) +
		((UV) s[uoffset + 1] << 16) +
		(     s[uoffset + 2] <<  8) +
		      s[uoffset + 3];
#ifdef UV_IS_QUAD
	else if (size == 64) {
	    Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
			   "Bit vector size > 32 non-portable");
	    retnum =
		((UV) s[uoffset    ] << 56) +
		((UV) s[uoffset + 1] << 48) +
		((UV) s[uoffset + 2] << 40) +
		((UV) s[uoffset + 3] << 32) +
		((UV) s[uoffset + 4] << 24) +
		((UV) s[uoffset + 5] << 16) +
		(     s[uoffset + 6] <<  8) +
		      s[uoffset + 7];
	}
#endif
    }

    return retnum;
}

/* currently converts input to bytes if possible but doesn't sweat failures,
 * although it does ensure that the string it clobbers is not marked as
 * utf8-valid any more
 */
void
Perl_do_vecset(pTHX_ SV *sv)
{
    SSize_t offset, bitoffs = 0;
    int size;
    unsigned char *s;
    UV lval;
    I32 mask;
    STRLEN targlen;
    STRLEN len;
    SV * const targ = LvTARG(sv);

    PERL_ARGS_ASSERT_DO_VECSET;

    if (!targ)
	return;
    s = (unsigned char*)SvPV_force_flags(targ, targlen,
                                         SV_GMAGIC | SV_UNDEF_RETURNS_NULL);
    if (SvUTF8(targ)) {
	/* This is handled by the SvPOK_only below...
	if (!Perl_sv_utf8_downgrade(aTHX_ targ, TRUE))
	    SvUTF8_off(targ);
	 */
	(void) Perl_sv_utf8_downgrade(aTHX_ targ, TRUE);
    }

    (void)SvPOK_only(targ);
    lval = SvUV(sv);
    offset = LvTARGOFF(sv);
    if (offset < 0)
	Perl_croak(aTHX_ "Negative offset to vec in lvalue context");
    size = LvTARGLEN(sv);
    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    if (size < 8) {
	bitoffs = ((offset%8)*size)%8;
	offset /= 8/size;
    }
    else if (size > 8)
	offset *= size/8;

    len = offset + (bitoffs + size + 7)/8;	/* required number of bytes */
    if (len > targlen) {
	s = (unsigned char*)SvGROW(targ, len + 1);
	(void)memzero((char *)(s + targlen), len - targlen + 1);
	SvCUR_set(targ, len);
    }

    if (size < 8) {
	mask = (1 << size) - 1;
	lval &= mask;
	s[offset] &= ~(mask << bitoffs);
	s[offset] |= lval << bitoffs;
    }
    else {
	if (size == 8)
	    s[offset  ] = (U8)( lval        & 0xff);
	else if (size == 16) {
	    s[offset  ] = (U8)((lval >>  8) & 0xff);
	    s[offset+1] = (U8)( lval        & 0xff);
	}
	else if (size == 32) {
	    s[offset  ] = (U8)((lval >> 24) & 0xff);
	    s[offset+1] = (U8)((lval >> 16) & 0xff);
	    s[offset+2] = (U8)((lval >>  8) & 0xff);
	    s[offset+3] = (U8)( lval        & 0xff);
	}
#ifdef UV_IS_QUAD
	else if (size == 64) {
	    Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
			   "Bit vector size > 32 non-portable");
	    s[offset  ] = (U8)((lval >> 56) & 0xff);
	    s[offset+1] = (U8)((lval >> 48) & 0xff);
	    s[offset+2] = (U8)((lval >> 40) & 0xff);
	    s[offset+3] = (U8)((lval >> 32) & 0xff);
	    s[offset+4] = (U8)((lval >> 24) & 0xff);
	    s[offset+5] = (U8)((lval >> 16) & 0xff);
	    s[offset+6] = (U8)((lval >>  8) & 0xff);
	    s[offset+7] = (U8)( lval        & 0xff);
	}
#endif
    }
    SvSETMAGIC(targ);
}

void
Perl_do_vop(pTHX_ I32 optype, SV *sv, SV *left, SV *right)
{
#ifdef LIBERAL
    long *dl;
    long *ll;
    long *rl;
#endif
    char *dc;
    STRLEN leftlen;
    STRLEN rightlen;
    const char *lc;
    const char *rc;
    STRLEN len;
    STRLEN lensave;
    const char *lsave;
    const char *rsave;
    bool left_utf;
    bool right_utf;
    bool do_warn_above_ff = ckWARN_d(WARN_DEPRECATED);
    STRLEN needlen = 0;

    PERL_ARGS_ASSERT_DO_VOP;

    if (sv != left || (optype != OP_BIT_AND && !SvOK(sv)))
	sv_setpvs(sv, "");	/* avoid undef warning on |= and ^= */
    if (sv == left) {
	lsave = lc = SvPV_force_nomg(left, leftlen);
    }
    else {
	lsave = lc = SvPV_nomg_const(left, leftlen);
	SvPV_force_nomg_nolen(sv);
    }
    rsave = rc = SvPV_nomg_const(right, rightlen);

    /* This needs to come after SvPV to ensure that string overloading has
       fired off.  */

    left_utf = DO_UTF8(left);
    right_utf = DO_UTF8(right);

    if (left_utf && !right_utf) {
	/* Avoid triggering overloading again by using temporaries.
	   Maybe there should be a variant of sv_utf8_upgrade that takes pvn
	*/
	right = newSVpvn_flags(rsave, rightlen, SVs_TEMP);
	sv_utf8_upgrade(right);
	rsave = rc = SvPV_nomg_const(right, rightlen);
	right_utf = TRUE;
    }
    else if (!left_utf && right_utf) {
	left = newSVpvn_flags(lsave, leftlen, SVs_TEMP);
	sv_utf8_upgrade(left);
	lsave = lc = SvPV_nomg_const(left, leftlen);
	left_utf = TRUE;
    }

    len = leftlen < rightlen ? leftlen : rightlen;
    lensave = len;
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);
    if ((left_utf || right_utf) && (sv == left || sv == right)) {
	needlen = optype == OP_BIT_AND ? len : leftlen + rightlen;
	Newxz(dc, needlen + 1, char);
    }
    else if (SvOK(sv) || SvTYPE(sv) > SVt_PVMG) {
	dc = SvPV_force_nomg_nolen(sv);
	if (SvLEN(sv) < len + 1) {
	    dc = SvGROW(sv, len + 1);
	    (void)memzero(dc + SvCUR(sv), len - SvCUR(sv) + 1);
	}
	if (optype != OP_BIT_AND && (left_utf || right_utf))
	    dc = SvGROW(sv, leftlen + rightlen + 1);
    }
    else {
	needlen = optype == OP_BIT_AND
		    ? len : (leftlen > rightlen ? leftlen : rightlen);
	Newxz(dc, needlen + 1, char);
	sv_usepvn_flags(sv, dc, needlen, SV_HAS_TRAILING_NUL);
	dc = SvPVX(sv);		/* sv_usepvn() calls Renew() */
    }
    if (left_utf || right_utf) {
	UV duc, luc, ruc;
	char *dcorig = dc;
	char *dcsave = NULL;
	STRLEN lulen = leftlen;
	STRLEN rulen = rightlen;
	STRLEN ulen;

	switch (optype) {
	case OP_BIT_AND:
	    while (lulen && rulen) {
		luc = utf8n_to_uvchr((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8n_to_uvchr((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc & ruc;
		dc = (char*)uvchr_to_utf8((U8*)dc, duc);
                if (do_warn_above_ff && (luc > 0xff || ruc > 0xff)) {
                    Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),
                                deprecated_above_ff_msg, PL_op_desc[optype]);
                    /* Warn only once per operation */
                    do_warn_above_ff = FALSE;
                }
	    }
	    if (sv == left || sv == right)
		(void)sv_usepvn(sv, dcorig, needlen);
	    SvCUR_set(sv, dc - dcorig);
	    break;
	case OP_BIT_XOR:
	    while (lulen && rulen) {
		luc = utf8n_to_uvchr((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8n_to_uvchr((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc ^ ruc;
		dc = (char*)uvchr_to_utf8((U8*)dc, duc);
                if (do_warn_above_ff && (luc > 0xff || ruc > 0xff)) {
                    Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),
                                deprecated_above_ff_msg, PL_op_desc[optype]);
                    do_warn_above_ff = FALSE;
                }
	    }
	    goto mop_up_utf;
	case OP_BIT_OR:
	    while (lulen && rulen) {
		luc = utf8n_to_uvchr((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8n_to_uvchr((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc | ruc;
		dc = (char*)uvchr_to_utf8((U8*)dc, duc);
                if (do_warn_above_ff && (luc > 0xff || ruc > 0xff)) {
                    Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),
                                deprecated_above_ff_msg, PL_op_desc[optype]);
                    do_warn_above_ff = FALSE;
                }
	    }
	  mop_up_utf:
	    if (rulen)
		dcsave = savepvn(rc, rulen);
	    else if (lulen)
		dcsave = savepvn(lc, lulen);
	    if (sv == left || sv == right)
		(void)sv_usepvn(sv, dcorig, needlen); /* uses Renew(); defaults to nomg */
	    SvCUR_set(sv, dc - dcorig);
	    if (rulen)
		sv_catpvn_nomg(sv, dcsave, rulen);
	    else if (lulen)
		sv_catpvn_nomg(sv, dcsave, lulen);
	    else
		*SvEND(sv) = '\0';
	    Safefree(dcsave);
	    break;
	default:
	    if (sv == left || sv == right)
		Safefree(dcorig);
	    Perl_croak(aTHX_ "panic: do_vop called for op %u (%s)",
			(unsigned)optype, PL_op_name[optype]);
	}
	SvUTF8_on(sv);
	goto finish;
    }
    else
#ifdef LIBERAL
    if (len >= sizeof(long)*4 &&
	!((unsigned long)dc % sizeof(long)) &&
	!((unsigned long)lc % sizeof(long)) &&
	!((unsigned long)rc % sizeof(long)))	/* It's almost always aligned... */
    {
	const STRLEN remainder = len % (sizeof(long)*4);
	len /= (sizeof(long)*4);

	dl = (long*)dc;
	ll = (long*)lc;
	rl = (long*)rc;

	switch (optype) {
	case OP_BIT_AND:
	    while (len--) {
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
	    }
	    break;
	case OP_BIT_XOR:
	    while (len--) {
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
	    }
	    break;
	case OP_BIT_OR:
	    while (len--) {
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
	    }
	}

	dc = (char*)dl;
	lc = (char*)ll;
	rc = (char*)rl;

	len = remainder;
    }
#endif
    {
	switch (optype) {
	case OP_BIT_AND:
	    while (len--)
		*dc++ = *lc++ & *rc++;
	    *dc = '\0';
	    break;
	case OP_BIT_XOR:
	    while (len--)
		*dc++ = *lc++ ^ *rc++;
	    goto mop_up;
	case OP_BIT_OR:
	    while (len--)
		*dc++ = *lc++ | *rc++;
	  mop_up:
	    len = lensave;
	    if (rightlen > len)
		sv_catpvn_nomg(sv, rsave + len, rightlen - len);
	    else if (leftlen > (STRLEN)len)
		sv_catpvn_nomg(sv, lsave + len, leftlen - len);
	    else
		*SvEND(sv) = '\0';
	    break;
	}
    }
  finish:
    SvTAINT(sv);
}


/* used for: pp_keys(), pp_values() */

OP *
Perl_do_kv(pTHX)
{
    dSP;
    HV * const keys = MUTABLE_HV(POPs);
    HE *entry;
    SSize_t extend_size;
    const U8 gimme = GIMME_V;
    const I32 dokv =     (PL_op->op_type == OP_RV2HV || PL_op->op_type == OP_PADHV);
    /* op_type is OP_RKEYS/OP_RVALUES if pp_rkeys delegated to here */
    const I32 dokeys =   dokv || (PL_op->op_type == OP_KEYS);
    const I32 dovalues = dokv || (PL_op->op_type == OP_VALUES);

    (void)hv_iterinit(keys);	/* always reset iterator regardless */

    if (gimme == G_VOID)
	RETURN;

    if (gimme == G_SCALAR) {
	if (PL_op->op_flags & OPf_MOD || LVRET) {	/* lvalue */
	    SV * const ret = sv_2mortal(newSV_type(SVt_PVLV));  /* Not TARG RT#67838 */
	    sv_magic(ret, NULL, PERL_MAGIC_nkeys, NULL, 0);
	    LvTYPE(ret) = 'k';
	    LvTARG(ret) = SvREFCNT_inc_simple(keys);
	    PUSHs(ret);
	}
	else {
	    IV i;
	    dTARGET;

	    if (! SvTIED_mg((const SV *)keys, PERL_MAGIC_tied) ) {
		i = HvUSEDKEYS(keys);
	    }
	    else {
		i = 0;
		while (hv_iternext(keys)) i++;
	    }
	    PUSHi( i );
	}
	RETURN;
    }

    /* 2*HvUSEDKEYS() should never be big enough to truncate or wrap */
    assert(HvUSEDKEYS(keys) <= (SSize_t_MAX >> 1));
    extend_size = (SSize_t)HvUSEDKEYS(keys) * (dokeys + dovalues);
    EXTEND(SP, extend_size);

    while ((entry = hv_iternext(keys))) {
	if (dokeys) {
	    SV* const sv = hv_iterkeysv(entry);
	    XPUSHs(sv);
	}
	if (dovalues) {
	    SV *tmpstr = hv_iterval(keys,entry);
	    DEBUG_H(Perl_sv_setpvf(aTHX_ tmpstr, "%lu%%%d=%lu",
			    (unsigned long)HeHASH(entry),
			    (int)HvMAX(keys)+1,
			    (unsigned long)(HeHASH(entry) & HvMAX(keys))));
	    XPUSHs(tmpstr);
	}
    }
    RETURN;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
