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


/* Helper function for do_trans().
 * Handles non-utf8 cases(*) not involving the /c, /d, /s flags,
 * and where search and replacement charlists aren't identical.
 * (*) i.e. where the search and replacement charlists are non-utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_simple(pTHX_ SV * const sv)
{
    Size_t matches = 0;
    STRLEN len;
    U8 *s = (U8*)SvPV_nomg(sv,len);
    U8 * const send = s+len;
    const OPtrans_map * const tbl = (OPtrans_map*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_SIMPLE;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_simple line %d",__LINE__);

    /* First, take care of non-UTF-8 input strings, because they're easy */
    if (!SvUTF8(sv)) {
	while (s < send) {
	    const short ch = tbl->map[*s];
	    if (ch >= 0) {
		matches++;
		*s = (U8)ch;
	    }
	    s++;
	}
	SvSETMAGIC(sv);
    }
    else {
	const bool grows = cBOOL(PL_op->op_private & OPpTRANS_GROWS);
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
	    short ch;

	    /* Need to check this, otherwise 128..255 won't match */
	    const UV c = utf8n_to_uvchr(s, send - s, &ulen, UTF8_ALLOW_DEFAULT);
	    if (c < 0x100 && (ch = tbl->map[c]) >= 0) {
		matches++;
		d = uvchr_to_utf8(d, (UV)ch);
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


/* Helper function for do_trans().
 * Handles non-utf8 cases(*) where search and replacement charlists are
 * identical: so the string isn't modified, and only a count of modifiable
 * chars is needed.
 * Note that it doesn't handle /d or /s, since these modify the string
 * even if the replacement list is empty.
 * (*) i.e. where the search and replacement charlists are non-utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_count(pTHX_ SV * const sv)
{
    STRLEN len;
    const U8 *s = (const U8*)SvPV_nomg_const(sv, len);
    const U8 * const send = s + len;
    Size_t matches = 0;
    const OPtrans_map * const tbl = (OPtrans_map*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_COUNT;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_count line %d",__LINE__);

    if (!SvUTF8(sv)) {
	while (s < send) {
            if (tbl->map[*s++] >= 0)
                matches++;
	}
    }
    else {
	const bool complement = cBOOL(PL_op->op_private & OPpTRANS_COMPLEMENT);
	while (s < send) {
	    STRLEN ulen;
	    const UV c = utf8n_to_uvchr(s, send - s, &ulen, UTF8_ALLOW_DEFAULT);
	    if (c < 0x100) {
		if (tbl->map[c] >= 0)
		    matches++;
	    } else if (complement)
		matches++;
	    s += ulen;
	}
    }

    return matches;
}


/* Helper function for do_trans().
 * Handles non-utf8 cases(*) involving the /c, /d, /s flags,
 * and where search and replacement charlists aren't identical.
 * (*) i.e. where the search and replacement charlists are non-utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_complex(pTHX_ SV * const sv)
{
    STRLEN len;
    U8 *s = (U8*)SvPV_nomg(sv, len);
    U8 * const send = s+len;
    Size_t matches = 0;
    const OPtrans_map * const tbl = (OPtrans_map*)cPVOP->op_pv;

    PERL_ARGS_ASSERT_DO_TRANS_COMPLEX;

    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_complex line %d",__LINE__);

    if (!SvUTF8(sv)) {
	U8 *d = s;
	U8 * const dstart = d;

	if (PL_op->op_private & OPpTRANS_SQUASH) {
	    const U8* p = send;
	    while (s < send) {
		const short ch = tbl->map[*s];
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
		const short ch = tbl->map[*s];
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
	const bool squash = cBOOL(PL_op->op_private & OPpTRANS_SQUASH);
	const bool grows  = cBOOL(PL_op->op_private & OPpTRANS_GROWS);
	U8 *d;
	U8 *dstart;
	Size_t size = tbl->size;
        UV pch = 0xfeedface;

	if (grows)
	    Newx(d, len*2+1, U8);
	else
	    d = s;
	dstart = d;

        while (s < send) {
            STRLEN len;
            const UV comp = utf8n_to_uvchr(s, send - s, &len,
                                           UTF8_ALLOW_DEFAULT);
            UV     ch;
            short sch;

            sch = tbl->map[comp >= size ? size : comp];

            if (sch >= 0) {
                ch = (UV)sch;
              replace:
                matches++;
                if (LIKELY(!squash || ch != pch)) {
                    d = uvchr_to_utf8(d, ch);
                    pch = ch;
                }
                s += len;
                continue;
            }
            else if (sch == -1) {	/* -1 is unmapped character */
                Move(s, d, len, U8);
                d += len;
            }
            else if (sch == -2)     /* -2 is delete character */
                matches++;
            else {
                assert(sch == -3);  /* -3 is empty replacement */
                ch = comp;
                goto replace;
            }

            s += len;
            pch = 0xfeedface;
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


/* Helper function for do_trans().
 * Handles utf8 cases(*) not involving the /c, /d, /s flags,
 * and where search and replacement charlists aren't identical.
 * (*) i.e. where the search or replacement charlists are utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_simple_utf8(pTHX_ SV * const sv)
{
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *start;
    U8 *dstart, *dend;
    Size_t matches = 0;
    const bool grows = cBOOL(PL_op->op_private & OPpTRANS_GROWS);
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
        hibit = ! is_utf8_invariant_string(s, len);
        if (hibit) {
            s = bytes_to_utf8(s, &len);
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


/* Helper function for do_trans().
 * Handles utf8 cases(*) where search and replacement charlists are
 * identical: so the string isn't modified, and only a count of modifiable
 * chars is needed.
 * Note that it doesn't handle /d or /s, since these modify the string
 * even if the replacement charlist is empty.
 * (*) i.e. where the search or replacement charlists are utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_count_utf8(pTHX_ SV * const sv)
{
    const U8 *s;
    const U8 *start = NULL;
    const U8 *send;
    Size_t matches = 0;
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
        hibit = ! is_utf8_invariant_string(s, len);
        if (hibit) {
            start = s = bytes_to_utf8(s, &len);
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


/* Helper function for do_trans().
 * Handles utf8 cases(*) involving the /c, /d, /s flags,
 * and where search and replacement charlists aren't identical.
 * (*) i.e. where the search or replacement charlists are utf8. sv may
 * or may not be utf8.
 */

STATIC Size_t
S_do_trans_complex_utf8(pTHX_ SV * const sv)
{
    U8 *start, *send;
    U8 *d;
    Size_t matches = 0;
    const bool squash   = cBOOL(PL_op->op_private & OPpTRANS_SQUASH);
    const bool del      = cBOOL(PL_op->op_private & OPpTRANS_DELETE);
    const bool grows    = cBOOL(PL_op->op_private & OPpTRANS_GROWS);
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
        hibit = ! is_utf8_invariant_string(s, len);
        if (hibit) {
            s = bytes_to_utf8(s, &len);
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


/* Execute a tr//. sv is the value to be translated, while PL_op
 * should be an OP_TRANS or OP_TRANSR op, whose op_pv field contains a
 * translation table or whose op_sv field contains a swash.
 * Returns a count of number of characters translated
 */

Size_t
Perl_do_trans(pTHX_ SV *sv)
{
    STRLEN len;
    const U8 flags = PL_op->op_private;
    const U8 hasutf = flags & (OPpTRANS_FROM_UTF | OPpTRANS_TO_UTF);

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

    SvPVCLEAR(sv);
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
Perl_do_sprintf(pTHX_ SV *sv, SSize_t len, SV **sarg)
{
    STRLEN patlen;
    const char * const pat = SvPV_const(*sarg, patlen);
    bool do_taint = FALSE;

    PERL_ARGS_ASSERT_DO_SPRINTF;
    assert(len >= 1);

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
    sv_vsetpvfn(sv, pat, patlen, NULL, sarg + 1, (Size_t)(len - 1), &do_taint);
    SvSETMAGIC(sv);
    if (do_taint)
	SvTAINTED_on(sv);
}

/* currently converts input to bytes if possible, but doesn't sweat failure */
UV
Perl_do_vecget(pTHX_ SV *sv, STRLEN offset, int size)
{
    STRLEN srclen, len, avail, uoffset, bitoffs = 0;
    const I32 svpv_flags = ((PL_op->op_flags & OPf_MOD || LVRET)
                                          ? SV_UNDEF_RETURNS_NULL : 0);
    unsigned char *s = (unsigned char *)
                            SvPV_flags(sv, srclen, (svpv_flags|SV_GMAGIC));
    UV retnum = 0;

    if (!s) {
      s = (unsigned char *)"";
    }
    
    PERL_ARGS_ASSERT_DO_VECGET;

    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    if (SvUTF8(sv)) {
	if (Perl_sv_utf8_downgrade(aTHX_ sv, TRUE)) {
            /* PVX may have changed */
            s = (unsigned char *) SvPV_flags(sv, srclen, svpv_flags);
        }
        else {
            Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
                                "Use of strings with code points over 0xFF as"
                                " arguments to vec is deprecated. This will"
                                " be a fatal error in Perl 5.32");
        }
    }

    if (size < 8) {
	bitoffs = ((offset%8)*size)%8;
	uoffset = offset/(8/size);
    }
    else if (size > 8) {
	int n = size/8;
        if (offset > Size_t_MAX / n - 1) /* would overflow */
            return 0;
	uoffset = offset*n;
    }
    else
	uoffset = offset;

    if (uoffset >= srclen)
        return 0;

    len   = (bitoffs + size + 7)/8; /* required number of bytes */
    avail = srclen - uoffset;       /* available number of bytes */

    /* Does the byte range overlap the end of the string? If so,
     * handle specially. */
    if (avail < len) {
	if (size <= 8)
	    retnum = 0;
	else {
	    if (size == 16) {
                assert(avail == 1);
                retnum = (UV) s[uoffset] <<  8;
	    }
	    else if (size == 32) {
                assert(avail >= 1 && avail <= 3);
		if (avail == 1)
		    retnum =
			((UV) s[uoffset    ] << 24);
		else if (avail == 2)
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
                assert(avail >= 1 && avail <= 7);
		if (avail == 1)
		    retnum =
			(UV) s[uoffset     ] << 56;
		else if (avail == 2)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48);
		else if (avail == 3)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40);
		else if (avail == 4)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32);
		else if (avail == 5)
		    retnum =
			((UV) s[uoffset    ] << 56) +
			((UV) s[uoffset + 1] << 48) +
			((UV) s[uoffset + 2] << 40) +
			((UV) s[uoffset + 3] << 32) +
			((UV) s[uoffset + 4] << 24);
		else if (avail == 6)
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
    STRLEN offset, bitoffs = 0;
    int size;
    unsigned char *s;
    UV lval;
    I32 mask;
    STRLEN targlen;
    STRLEN len;
    SV * const targ = LvTARG(sv);
    char errflags = LvFLAGS(sv);

    PERL_ARGS_ASSERT_DO_VECSET;

    /* some out-of-range errors have been deferred if/until the LV is
     * actually written to: f(vec($s,-1,8)) is not always fatal */
    if (errflags) {
        assert(!(errflags & ~(LVf_NEG_OFF|LVf_OUT_OF_RANGE)));
        if (errflags & LVf_NEG_OFF)
            Perl_croak_nocontext("Negative offset to vec in lvalue context");
        Perl_croak_nocontext("Out of memory!");
    }

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
    size = LvTARGLEN(sv);

    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    if (size < 8) {
	bitoffs = ((offset%8)*size)%8;
	offset /= 8/size;
    }
    else if (size > 8) {
	int n = size/8;
        if (offset > Size_t_MAX / n - 1) /* would overflow */
            Perl_croak_nocontext("Out of memory!");
	offset *= n;
    }

    len = (bitoffs + size + 7)/8;	/* required number of bytes */
    if (targlen < offset || targlen - offset < len) {
        STRLEN newlen = offset > Size_t_MAX - len - 1 ? /* avoid overflow */
                                        Size_t_MAX : offset + len + 1;
	s = (unsigned char*)SvGROW(targ, newlen);
	(void)memzero((char *)(s + targlen), newlen - targlen);
	SvCUR_set(targ, newlen - 1);
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
    STRLEN len = 0;
    STRLEN lensave;
    const char *lsave;
    const char *rsave;
    STRLEN needlen = 0;
    bool result_needs_to_be_utf8 = FALSE;
    bool left_utf8 = FALSE;
    bool right_utf8 = FALSE;
    U8 * left_non_downgraded = NULL;
    U8 * right_non_downgraded = NULL;
    Size_t left_non_downgraded_len = 0;
    Size_t right_non_downgraded_len = 0;
    char * non_downgraded = NULL;
    Size_t non_downgraded_len = 0;

    PERL_ARGS_ASSERT_DO_VOP;

    if (sv != left || (optype != OP_BIT_AND && !SvOK(sv)))
        SvPVCLEAR(sv);        /* avoid undef warning on |= and ^= */
    if (sv == left) {
	lc = SvPV_force_nomg(left, leftlen);
    }
    else {
	lc = SvPV_nomg_const(left, leftlen);
	SvPV_force_nomg_nolen(sv);
    }
    rc = SvPV_nomg_const(right, rightlen);

    /* This needs to come after SvPV to ensure that string overloading has
       fired off.  */

    /* Create downgraded temporaries of any UTF-8 encoded operands */
    if (DO_UTF8(left)) {
        const U8 * save_lc = (U8 *) lc;

        left_utf8 = TRUE;
        result_needs_to_be_utf8 = TRUE;

        left_non_downgraded_len = leftlen;
        lc = (char *) bytes_from_utf8_loc((const U8 *) lc, &leftlen,
                                          &left_utf8,
                                          (const U8 **) &left_non_downgraded);
        /* Calculate the number of trailing unconvertible bytes.  This quantity
         * is the original length minus the length of the converted portion. */
        left_non_downgraded_len -= left_non_downgraded - save_lc;
        SAVEFREEPV(lc);
    }
    if (DO_UTF8(right)) {
        const U8 * save_rc = (U8 *) rc;

        right_utf8 = TRUE;
        result_needs_to_be_utf8 = TRUE;

        right_non_downgraded_len = rightlen;
        rc = (char *) bytes_from_utf8_loc((const U8 *) rc, &rightlen,
                                          &right_utf8,
                                          (const U8 **) &right_non_downgraded);
        right_non_downgraded_len -= right_non_downgraded - save_rc;
        SAVEFREEPV(rc);
    }

    /* We set 'len' to the length that the operation actually operates on.  The
     * dangling part of the longer operand doesn't actually participate in the
     * operation.  What happens is that we pretend that the shorter operand has
     * been extended to the right by enough imaginary zeros to match the length
     * of the longer one.  But we know in advance the result of the operation
     * on zeros without having to do it.  In the case of '&', the result is
     * zero, and the dangling portion is simply discarded.  For '|' and '^', the
     * result is the same as the other operand, so the dangling part is just
     * appended to the final result, unchanged.  We currently accept above-FF
     * code points in the dangling portion, as that's how it has long worked,
     * and code depends on it staying that way.  But it is now fatal for
     * above-FF to appear in the portion that does get operated on.  Hence, any
     * above-FF must come only in the longer operand, and only in its dangling
     * portion.  That means that at least one of the operands has to be
     * entirely non-UTF-8, and the length of that operand has to be before the
     * first above-FF in the other */
    if (left_utf8 || right_utf8) {
        if (left_utf8) {
            if (right_utf8 || rightlen > leftlen) {
                Perl_croak(aTHX_ FATAL_ABOVE_FF_MSG, PL_op_desc[optype]);
            }
            len = rightlen;
        }
        else if (right_utf8) {
            if (leftlen > rightlen) {
                Perl_croak(aTHX_ FATAL_ABOVE_FF_MSG, PL_op_desc[optype]);
            }
            len = leftlen;
        }

        Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
                               DEPRECATED_ABOVE_FF_MSG, PL_op_desc[optype]);
    }
    else {  /* Neither is UTF-8 */
        len = MIN(leftlen, rightlen);
    }

    lensave = len;
    lsave = lc;
    rsave = rc;

    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);
    if (SvOK(sv) || SvTYPE(sv) > SVt_PVMG) {
	dc = SvPV_force_nomg_nolen(sv);
	if (SvLEN(sv) < len + 1) {
	    dc = SvGROW(sv, len + 1);
	    (void)memzero(dc + SvCUR(sv), len - SvCUR(sv) + 1);
	}
    }
    else {
	needlen = optype == OP_BIT_AND
		    ? len : (leftlen > rightlen ? leftlen : rightlen);
	Newxz(dc, needlen + 1, char);
	sv_usepvn_flags(sv, dc, needlen, SV_HAS_TRAILING_NUL);
	dc = SvPVX(sv);		/* sv_usepvn() calls Renew() */
    }

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
        if (rightlen > len) {
            if (dc == rc)
                SvCUR(sv) = rightlen;
            else
                sv_catpvn_nomg(sv, rsave + len, rightlen - len);
        }
        else if (leftlen > len) {
            if (dc == lc)
                SvCUR(sv) = leftlen;
            else
                sv_catpvn_nomg(sv, lsave + len, leftlen - len);
        }
        *SvEND(sv) = '\0';

        /* If there is trailing stuff that couldn't be converted from UTF-8, it
         * is appended as-is for the ^ and | operators.  This preserves
         * backwards compatibility */
        if (right_non_downgraded) {
            non_downgraded = (char *) right_non_downgraded;
            non_downgraded_len = right_non_downgraded_len;
        }
        else if (left_non_downgraded) {
            non_downgraded = (char *) left_non_downgraded;
            non_downgraded_len = left_non_downgraded_len;
        }

        break;
    }

    if (result_needs_to_be_utf8) {
        sv_utf8_upgrade_nomg(sv);

        /* Append any trailing UTF-8 as-is. */
        if (non_downgraded) {
            sv_catpvn_nomg(sv, non_downgraded, non_downgraded_len);
        }
    }

    SvTAINT(sv);
}


/* Perl_do_kv() may be:
 *  * called directly as the pp function for pp_keys() and pp_values();
 *  * It may also be called directly when the op is OP_AVHVSWITCH, to
 *       implement CORE::keys(), CORE::values().
 *
 * In all cases it expects an HV on the stack and returns a list of keys,
 * values, or key-value pairs, depending on PL_op.
 */

OP *
Perl_do_kv(pTHX)
{
    dSP;
    HV * const keys = MUTABLE_HV(POPs);
    const U8 gimme = GIMME_V;

    const I32 dokeys   =     (PL_op->op_type == OP_KEYS)
                          || (    PL_op->op_type == OP_AVHVSWITCH
                              && (PL_op->op_private & OPpAVHVSWITCH_MASK)
                                    + OP_EACH == OP_KEYS);

    const I32 dovalues =     (PL_op->op_type == OP_VALUES)
                          || (    PL_op->op_type == OP_AVHVSWITCH
                              && (PL_op->op_private & OPpAVHVSWITCH_MASK)
                                     + OP_EACH == OP_VALUES);

    assert(   PL_op->op_type == OP_KEYS
           || PL_op->op_type == OP_VALUES
           || PL_op->op_type == OP_AVHVSWITCH);

    assert(!(    PL_op->op_type == OP_VALUES
             && (PL_op->op_private & OPpMAYBE_LVSUB)));

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

            /* note that in 'scalar(keys %h)' the OP_KEYS is usually
             * optimised away and the action is performed directly by the
             * padhv or rv2hv op. We now only get here via OP_AVHVSWITCH
             * and \&CORE::keys
             */
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

    if (UNLIKELY(PL_op->op_private & OPpMAYBE_LVSUB)) {
	const I32 flags = is_lvalue_sub();
	if (flags && !(flags & OPpENTERSUB_INARGS))
	    /* diag_listed_as: Can't modify %s in %s */
	    Perl_croak(aTHX_ "Can't modify keys in list assignment");
    }

    PUTBACK;
    hv_pushkv(keys, (dokeys | (dovalues << 1)));
    return NORMAL;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
