#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef USE_PPPORT_H
#  define NEED_my_snprintf
#  define NEED_sv_2pv_flags
#  include "ppport.h"
#endif

#if PERL_VERSION < 8
#  define DD_USE_OLD_ID_FORMAT
#endif

#ifndef strlcpy
#  ifdef my_strlcpy
#    define strlcpy(d,s,l) my_strlcpy(d,s,l)
#  else
#    define strlcpy(d,s,l) strcpy(d,s)
#  endif
#endif

/* These definitions are ASCII only.  But the pure-perl .pm avoids
 * calling this .xs file for releases where they aren't defined */

#ifndef isASCII
#   define isASCII(c) (((UV) (c)) < 128)
#endif

#ifndef ESC_NATIVE          /* \e */
#   define ESC_NATIVE 27
#endif

#ifndef isPRINT
#   define isPRINT(c) (((UV) (c)) >= ' ' && ((UV) (c)) < 127)
#endif

#ifndef isALPHA
#   define isALPHA(c) (   (((UV) (c)) >= 'a' && ((UV) (c)) <= 'z')          \
                       || (((UV) (c)) <= 'Z' && ((UV) (c)) >= 'A'))
#endif

#ifndef isIDFIRST
#   define isIDFIRST(c) (isALPHA(c) || (c) == '_')
#endif

#ifndef isWORDCHAR
#   define isWORDCHAR(c) (isIDFIRST(c)                                      \
                          || (((UV) (c)) >= '0' && ((UV) (c)) <= '9'))
#endif

/* SvPVCLEAR only from perl 5.25.6 */
#ifndef SvPVCLEAR
#  define SvPVCLEAR(sv) sv_setpvs((sv), "")
#endif

#ifndef memBEGINs
#  define memBEGINs(s1, l, s2)                                              \
            (   (l) >= sizeof(s2) - 1                                       \
             && memEQ(s1, "" s2 "", sizeof(s2)-1))
#endif

/* This struct contains almost all the user's desired configuration, and it
 * is treated as constant by the recursive function. This arrangement has
 * the advantage of needing less memory than passing all of them on the
 * stack all the time (as was the case in an earlier implementation). */
typedef struct {
    SV *pad;
    SV *xpad;
    SV *sep;
    SV *pair;
    SV *sortkeys;
    SV *freezer;
    SV *toaster;
    SV *bless;
    IV maxrecurse;
    I32 indent;
    I32 purity;
    I32 deepcopy;
    I32 quotekeys;
    I32 maxdepth;
    I32 useqq;
    int use_sparse_seen_hash;
    int trailingcomma;
    int deparse;
} Style;

static STRLEN num_q (const char *s, STRLEN slen);
static STRLEN esc_q (char *dest, const char *src, STRLEN slen);
static STRLEN esc_q_utf8 (pTHX_ SV *sv, const char *src, STRLEN slen, I32 do_utf8, I32 useqq);
static bool globname_needs_quote(const char *s, STRLEN len);
static bool key_needs_quote(const char *s, STRLEN len);
static bool safe_decimal_number(const char *p, STRLEN len);
static SV *sv_x (pTHX_ SV *sv, const char *str, STRLEN len, I32 n);
static I32 DD_dump (pTHX_ SV *val, const char *name, STRLEN namelen, SV *retval,
                    HV *seenhv, AV *postav, const I32 level, SV *apad,
                    const Style *style);

#ifndef HvNAME_get
#define HvNAME_get HvNAME
#endif

/* Perls 7 through portions of 15 used utf8_to_uvchr() which didn't have a
 * length parameter.  This wrongly allowed reading beyond the end of buffer
 * given malformed input */

#if PERL_VERSION <= 6 /* Perl 5.6 and earlier */

UV
Perl_utf8_to_uvchr_buf(pTHX_ U8 *s, U8 *send, STRLEN *retlen)
{
    const UV uv = utf8_to_uv(s, send - s, retlen,
                    ckWARN(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY);
    return UNI_TO_NATIVE(uv);
}

# if !defined(PERL_IMPLICIT_CONTEXT)
#  define utf8_to_uvchr_buf	     Perl_utf8_to_uvchr_buf
# else
#  define utf8_to_uvchr_buf(a,b,c) Perl_utf8_to_uvchr_buf(aTHX_ a,b,c)
# endif

#endif /* PERL_VERSION <= 6 */

/* Perl 5.7 through part of 5.15 */
#if PERL_VERSION > 6 && PERL_VERSION <= 15 && ! defined(utf8_to_uvchr_buf)

UV
Perl_utf8_to_uvchr_buf(pTHX_ U8 *s, U8 *send, STRLEN *retlen)
{
    /* We have to discard <send> for these versions; hence can read off the
     * end of the buffer if there is a malformation that indicates the
     * character is longer than the space available */

    return utf8_to_uvchr(s, retlen);
}

# if !defined(PERL_IMPLICIT_CONTEXT)
#  define utf8_to_uvchr_buf	     Perl_utf8_to_uvchr_buf
# else
#  define utf8_to_uvchr_buf(a,b,c) Perl_utf8_to_uvchr_buf(aTHX_ a,b,c)
# endif

#endif /* PERL_VERSION > 6 && <= 15 */

/* Changes in 5.7 series mean that now IOK is only set if scalar is
   precisely integer but in 5.6 and earlier we need to do a more
   complex test  */
#if PERL_VERSION <= 6
#define DD_is_integer(sv) (SvIOK(sv) && (SvIsUV(val) ? SvUV(sv) == SvNV(sv) : SvIV(sv) == SvNV(sv)))
#else
#define DD_is_integer(sv) SvIOK(sv)
#endif

/* does a glob name need to be protected? */
static bool
globname_needs_quote(const char *s, STRLEN len)
{
    const char *send = s+len;
TOP:
    if (s[0] == ':') {
	if (++s<send) {
	    if (*s++ != ':')
                return TRUE;
	}
	else
	    return TRUE;
    }
    if (isIDFIRST(*s)) {
	while (++s<send)
	    if (!isWORDCHAR(*s)) {
		if (*s == ':')
		    goto TOP;
		else
                    return TRUE;
	    }
    }
    else
        return TRUE;

    return FALSE;
}

/* does a hash key need to be quoted (to the left of => ).
   Previously this used (globname_)needs_quote() which accepted strings
   like '::foo', but these aren't safe as unquoted keys under strict.
*/
static bool
key_needs_quote(const char *s, STRLEN len) {
    const char *send = s+len;

    if (safe_decimal_number(s, len)) {
        return FALSE;
    }
    else if (isIDFIRST(*s)) {
        while (++s<send)
            if (!isWORDCHAR(*s))
                return TRUE;
    }
    else
        return TRUE;

    return FALSE;
}

/* Check that the SV can be represented as a simple decimal integer.
 *
 * The perl code does this by matching against /^(?:0|-?[1-9]\d{0,8})\z/
*/
static bool
safe_decimal_number(const char *p, STRLEN len) {
    if (len == 1 && *p == '0')
        return TRUE;

    if (len && *p == '-') {
        ++p;
        --len;
    }

    if (len == 0 || *p < '1' || *p > '9')
        return FALSE;

    ++p;
    --len;

    if (len > 8)
        return FALSE;

    while (len > 0) {
         /* the perl code checks /\d/ but we don't want unicode digits here */
         if (*p < '0' || *p > '9')
             return FALSE;
         ++p;
         --len;
    }
    return TRUE;
}

/* count the number of "'"s and "\"s in string */
static STRLEN
num_q(const char *s, STRLEN slen)
{
    STRLEN ret = 0;

    while (slen > 0) {
	if (*s == '\'' || *s == '\\')
	    ++ret;
	++s;
	--slen;
    }
    return ret;
}


/* returns number of chars added to escape "'"s and "\"s in s */
/* slen number of characters in s will be escaped */
/* destination must be long enough for additional chars */
static STRLEN
esc_q(char *d, const char *s, STRLEN slen)
{
    STRLEN ret = 0;

    while (slen > 0) {
	switch (*s) {
	case '\'':
	case '\\':
	    *d = '\\';
	    ++d; ++ret;
            /* FALLTHROUGH */
	default:
	    *d = *s;
	    ++d; ++s; --slen;
	    break;
	}
    }
    return ret;
}

/* this function is also misused for implementing $Useqq */
static STRLEN
esc_q_utf8(pTHX_ SV* sv, const char *src, STRLEN slen, I32 do_utf8, I32 useqq)
{
    char *r, *rstart;
    const char *s = src;
    const char * const send = src + slen;
    STRLEN j, cur = SvCUR(sv);
    /* Could count 128-255 and 256+ in two variables, if we want to
       be like &qquote and make a distinction.  */
    STRLEN grow = 0;	/* bytes needed to represent chars 128+ */
    /* STRLEN topbit_grow = 0;	bytes needed to represent chars 128-255 */
    STRLEN backslashes = 0;
    STRLEN single_quotes = 0;
    STRLEN qq_escapables = 0;	/* " $ @ will need a \ in "" strings.  */
    STRLEN normal = 0;
    int increment;

    for (s = src; s < send; s += increment) { /* Sizing pass */
        UV k = *(U8*)s;

        increment = 1;      /* Will override if necessary for utf-8 */

        if (isPRINT(k)) {
            if (k == '\\') {
                backslashes++;
            } else if (k == '\'') {
                single_quotes++;
            } else if (k == '"' || k == '$' || k == '@') {
                qq_escapables++;
            } else {
                normal++;
            }
        }
        else if (! isASCII(k) && k > ' ') {
            /* High ordinal non-printable code point.  (The test that k is
             * above SPACE should be optimized out by the compiler on
             * non-EBCDIC platforms; otherwise we could put an #ifdef around
             * it, but it's better to have just a single code path when
             * possible.  All but one of the non-ASCII EBCDIC controls are low
             * ordinal; that one is the only one above SPACE.)
             *
             * If UTF-8, output as hex, regardless of useqq.  This means there
             * is an overhead of 4 chars '\x{}'.  Then count the number of hex
             * digits.  */
            if (do_utf8) {
                k = utf8_to_uvchr_buf((U8*)s, (U8*) send, NULL);

                /* treat invalid utf8 byte by byte.  This loop iteration gets the
                * first byte */
                increment = (k == 0 && *s != '\0') ? 1 : UTF8SKIP(s);

                grow += 4 + (k <= 0xFF ? 2 : k <= 0xFFF ? 3 : k <= 0xFFFF ? 4 :
#if UVSIZE == 4
                    8 /* We may allocate a bit more than the minimum here.  */
#else
                    k <= 0xFFFFFFFF ? 8 : UVSIZE * 4
#endif
                    );
            }
            else if (useqq) {   /* Not utf8, must be <= 0xFF, hence 2 hex
                                 * digits. */
                grow += 4 + 2;
            }
            else {  /* Non-qq generates 3 octal digits plus backslash */
                grow += 4;
            }
	} /* End of high-ordinal non-printable */
        else if (! useqq) { /* Low ordinal, non-printable, non-qq just
                             * outputs the raw char */
            normal++;
        }
        else {  /* Is qq, low ordinal, non-printable.  Output escape
                 * sequences */
            if (   k == '\a' || k == '\b' || k == '\t' || k == '\n' || k == '\r'
                || k == '\f' || k == ESC_NATIVE)
            {
                grow += 2;  /* 1 char plus backslash */
            }
            else /* The other low ordinals are output as an octal escape
                  * sequence */
                 if (s + 1 >= send || (   *(U8*)(s+1) >= '0'
                                       && *(U8*)(s+1) <= '9'))
            {
                /* When the following character is a digit, use 3 octal digits
                 * plus backslash, as using fewer digits would concatenate the
                 * following char into this one */
                grow += 4;
            }
            else if (k <= 7) {
                grow += 2;  /* 1 octal digit, plus backslash */
            }
            else if (k <= 077) {
                grow += 3;  /* 2 octal digits plus backslash */
            }
            else {
                grow += 4;  /* 3 octal digits plus backslash */
            }
        }
    } /* End of size-calculating loop */

    if (grow || useqq) {
        /* We have something needing hex. 3 is ""\0 */
        sv_grow(sv, cur + 3 + grow + 2*backslashes + single_quotes
		+ 2*qq_escapables + normal);
        rstart = r = SvPVX(sv) + cur;

        *r++ = '"';

        for (s = src; s < send; s += increment) {
            UV k;

            if (do_utf8
                && ! isASCII(*s)
                    /* Exclude non-ASCII low ordinal controls.  This should be
                     * optimized out by the compiler on ASCII platforms; if not
                     * could wrap it in a #ifdef EBCDIC, but better to avoid
                     * #if's if possible */
                && *(U8*)s > ' '
            ) {

                /* When in UTF-8, we output all non-ascii chars as \x{}
                 * reqardless of useqq, except for the low ordinal controls on
                 * EBCDIC platforms */
                k = utf8_to_uvchr_buf((U8*)s, (U8*) send, NULL);

                /* treat invalid utf8 byte by byte.  This loop iteration gets the
                * first byte */
                increment = (k == 0 && *s != '\0') ? 1 : UTF8SKIP(s);

#if PERL_VERSION < 10
                sprintf(r, "\\x{%" UVxf "}", k);
                r += strlen(r);
                /* my_sprintf is not supported by ppport.h */
#else
                r = r + my_sprintf(r, "\\x{%" UVxf "}", k);
#endif
                continue;
            }

            /* Here 1) isn't UTF-8; or
             *      2) the current character is ASCII; or
             *      3) it is an EBCDIC platform and is a low ordinal
             *         non-ASCII control.
             * In each case the character occupies just one byte */
            k = *(U8*)s;
            increment = 1;

            if (isPRINT(k)) {
                /* These need a backslash escape */
                if (k == '"' || k == '\\' || k == '$' || k == '@') {
                    *r++ = '\\';
                }

                *r++ = (char)k;
            }
            else if (! useqq) { /* non-qq, non-printable, low-ordinal is
                                 * output raw */
                *r++ = (char)k;
            }
            else {  /* Is qq means use escape sequences */
	        bool next_is_digit;

		*r++ = '\\';
		switch (k) {
		case '\a':  *r++ = 'a'; break;
		case '\b':  *r++ = 'b'; break;
		case '\t':  *r++ = 't'; break;
		case '\n':  *r++ = 'n'; break;
		case '\f':  *r++ = 'f'; break;
		case '\r':  *r++ = 'r'; break;
		case ESC_NATIVE: *r++ = 'e'; break;
		default:

		    /* only ASCII digits matter here, which are invariant,
		     * since we only encode characters \377 and under, or
		     * \x177 and under for a unicode string
		     */
                    next_is_digit = (s + 1 >= send )
                                    ? FALSE
                                    : (*(U8*)(s+1) >= '0' && *(U8*)(s+1) <= '9');

		    /* faster than
		     * r = r + my_sprintf(r, "%o", k);
		     */
		    if (k <= 7 && !next_is_digit) {
			*r++ = (char)k + '0';
		    } else if (k <= 63 && !next_is_digit) {
			*r++ = (char)(k>>3) + '0';
			*r++ = (char)(k&7) + '0';
		    } else {
			*r++ = (char)(k>>6) + '0';
			*r++ = (char)((k&63)>>3) + '0';
			*r++ = (char)(k&7) + '0';
		    }
		}
	    }
        }
        *r++ = '"';
    } else {
        /* Single quotes.  */
        sv_grow(sv, cur + 3 + 2*backslashes + 2*single_quotes
		+ qq_escapables + normal);
        rstart = r = SvPVX(sv) + cur;
        *r++ = '\'';
        for (s = src; s < send; s ++) {
            const char k = *s;
            if (k == '\'' || k == '\\')
                *r++ = '\\';
            *r++ = k;
        }
        *r++ = '\'';
    }
    *r = '\0';
    j = r - rstart;
    SvCUR_set(sv, cur + j);

    return j;
}

/* append a repeated string to an SV */
static SV *
sv_x(pTHX_ SV *sv, const char *str, STRLEN len, I32 n)
{
    if (!sv)
	sv = newSVpvs("");
#ifdef DEBUGGING
    else
	assert(SvTYPE(sv) >= SVt_PV);
#endif

    if (n > 0) {
	SvGROW(sv, len*n + SvCUR(sv) + 1);
	if (len == 1) {
	    char * const start = SvPVX(sv) + SvCUR(sv);
	    SvCUR_set(sv, SvCUR(sv) + n);
	    start[n] = '\0';
	    while (n > 0)
		start[--n] = str[0];
	}
	else
	    while (n > 0) {
		sv_catpvn(sv, str, len);
		--n;
	    }
    }
    return sv;
}

static SV *
deparsed_output(pTHX_ SV *val)
{
    SV *text;
    int n;
    dSP;

    /* This is passed to load_module(), which decrements its ref count and
     * modifies it (so we also can't reuse it below) */
    SV *pkg = newSVpvs("B::Deparse");

    load_module(PERL_LOADMOD_NOIMPORT, pkg, 0);

    SAVETMPS;

    PUSHMARK(SP);
    mXPUSHs(newSVpvs("B::Deparse"));
    PUTBACK;

    n = call_method("new", G_SCALAR);
    SPAGAIN;

    if (n != 1) {
        croak("B::Deparse->new returned %d items, but expected exactly 1", n);
    }

    PUSHMARK(SP - n);
    XPUSHs(val);
    PUTBACK;

    n = call_method("coderef2text", G_SCALAR);
    SPAGAIN;

    if (n != 1) {
        croak("$b_deparse->coderef2text returned %d items, but expected exactly 1", n);
    }

    text = POPs;
    SvREFCNT_inc(text);         /* the caller will mortalise this */

    FREETMPS;

    PUTBACK;

    return text;
}

/*
 * This ought to be split into smaller functions. (it is one long function since
 * it exactly parallels the perl version, which was one long thing for
 * efficiency raisins.)  Ugggh!
 */
static I32
DD_dump(pTHX_ SV *val, const char *name, STRLEN namelen, SV *retval, HV *seenhv,
	AV *postav, const I32 level, SV *apad, const Style *style)
{
    char tmpbuf[128];
    Size_t i;
    char *c, *r, *realpack;
#ifdef DD_USE_OLD_ID_FORMAT
    char id[128];
#else
    UV id_buffer;
    char *const id = (char *)&id_buffer;
#endif
    SV **svp;
    SV *sv, *ipad, *ival;
    SV *blesspad = Nullsv;
    AV *seenentry = NULL;
    char *iname;
    STRLEN inamelen, idlen = 0;
    U32 realtype;
    bool no_bless = 0; /* when a qr// is blessed into Regexp we dont want to bless it.
                          in later perls we should actually check the classname of the 
                          engine. this gets tricky as it involves lexical issues that arent so
                          easy to resolve */
    bool is_regex = 0; /* we are dumping a regex, we need to know this before we bless */

    if (!val)
	return 0;

    /* If the output buffer has less than some arbitrary amount of space
       remaining, then enlarge it. For the test case (25M of output),
       *1.1 was slower, *2.0 was the same, so the first guess of 1.5 is
	deemed to be good enough.  */
    if (SvTYPE(retval) >= SVt_PV && (SvLEN(retval) - SvCUR(retval)) < 42) {
	sv_grow(retval, SvCUR(retval) * 3 / 2);
    }

    realtype = SvTYPE(val);

    if (SvGMAGICAL(val))
        mg_get(val);
    if (SvROK(val)) {

        /* If a freeze method is provided and the object has it, call
           it.  Warn on errors. */
        if (SvOBJECT(SvRV(val)) && style->freezer &&
            SvPOK(style->freezer) && SvCUR(style->freezer) &&
            gv_fetchmeth(SvSTASH(SvRV(val)), SvPVX_const(style->freezer),
                         SvCUR(style->freezer), -1) != NULL)
	{
	    dSP; ENTER; SAVETMPS; PUSHMARK(sp);
	    XPUSHs(val); PUTBACK;
            i = perl_call_method(SvPVX_const(style->freezer), G_EVAL|G_VOID|G_DISCARD);
	    SPAGAIN;
	    if (SvTRUE(ERRSV))
		warn("WARNING(Freezer method call failed): %" SVf, ERRSV);
	    PUTBACK; FREETMPS; LEAVE;
	}
	
	ival = SvRV(val);
	realtype = SvTYPE(ival);
#ifdef DD_USE_OLD_ID_FORMAT
        idlen = my_snprintf(id, sizeof(id), "0x%" UVxf, PTR2UV(ival));
#else
	id_buffer = PTR2UV(ival);
	idlen = sizeof(id_buffer);
#endif
	if (SvOBJECT(ival))
	    realpack = HvNAME_get(SvSTASH(ival));
	else
	    realpack = NULL;

	/* if it has a name, we need to either look it up, or keep a tab
	 * on it so we know when we hit it later
	 */
	if (namelen) {
	    if ((svp = hv_fetch(seenhv, id, idlen, FALSE))
		&& (sv = *svp) && SvROK(sv) && (seenentry = (AV*)SvRV(sv)))
	    {
		SV *othername;
		if ((svp = av_fetch(seenentry, 0, FALSE))
		    && (othername = *svp))
		{
		    if (style->purity && level > 0) {
			SV *postentry;
			
			if (realtype == SVt_PVHV)
			    sv_catpvs(retval, "{}");
			else if (realtype == SVt_PVAV)
			    sv_catpvs(retval, "[]");
			else
			    sv_catpvs(retval, "do{my $o}");
			postentry = newSVpvn(name, namelen);
			sv_catpvs(postentry, " = ");
			sv_catsv(postentry, othername);
			av_push(postav, postentry);
		    }
		    else {
			if (name[0] == '@' || name[0] == '%') {
			    if ((SvPVX_const(othername))[0] == '\\' &&
				(SvPVX_const(othername))[1] == name[0]) {
				sv_catpvn(retval, SvPVX_const(othername)+1,
					  SvCUR(othername)-1);
			    }
			    else {
				sv_catpvn(retval, name, 1);
				sv_catpvs(retval, "{");
				sv_catsv(retval, othername);
				sv_catpvs(retval, "}");
			    }
			}
			else
			    sv_catsv(retval, othername);
		    }
		    return 1;
		}
		else {
#ifdef DD_USE_OLD_ID_FORMAT
		    warn("ref name not found for %s", id);
#else
		    warn("ref name not found for 0x%" UVxf, PTR2UV(ival));
#endif
		    return 0;
		}
	    }
	    else {   /* store our name and continue */
		SV *namesv;
		if (name[0] == '@' || name[0] == '%') {
		    namesv = newSVpvs("\\");
		    sv_catpvn(namesv, name, namelen);
		}
		else if (realtype == SVt_PVCV && name[0] == '*') {
		    namesv = newSVpvs("\\");
		    sv_catpvn(namesv, name, namelen);
		    (SvPVX(namesv))[1] = '&';
		}
		else
		    namesv = newSVpvn(name, namelen);
		seenentry = newAV();
		av_push(seenentry, namesv);
		(void)SvREFCNT_inc(val);
		av_push(seenentry, val);
		(void)hv_store(seenhv, id, idlen,
			       newRV_inc((SV*)seenentry), 0);
		SvREFCNT_dec(seenentry);
	    }
	}
        /* regexps dont have to be blessed into package "Regexp"
         * they can be blessed into any package. 
         */
#if PERL_VERSION < 8
	if (realpack && *realpack == 'R' && strEQ(realpack, "Regexp")) 
#elif PERL_VERSION < 11
        if (realpack && realtype == SVt_PVMG && mg_find(ival, PERL_MAGIC_qr))
#else        
        if (realpack && realtype == SVt_REGEXP) 
#endif
        {
            is_regex = 1;
            if (strEQ(realpack, "Regexp")) 
                no_bless = 1;
            else
                no_bless = 0;
        }

	/* If purity is not set and maxdepth is set, then check depth:
	 * if we have reached maximum depth, return the string
	 * representation of the thing we are currently examining
	 * at this depth (i.e., 'Foo=ARRAY(0xdeadbeef)').
	 */
        if (!style->purity && style->maxdepth > 0 && level >= style->maxdepth) {
	    STRLEN vallen;
	    const char * const valstr = SvPV(val,vallen);
	    sv_catpvs(retval, "'");
	    sv_catpvn(retval, valstr, vallen);
	    sv_catpvs(retval, "'");
	    return 1;
	}

        if (style->maxrecurse > 0 && level >= style->maxrecurse) {
            croak("Recursion limit of %" IVdf " exceeded", style->maxrecurse);
	}

	if (realpack && !no_bless) {				/* we have a blessed ref */
	    STRLEN blesslen;
            const char * const blessstr = SvPV(style->bless, blesslen);
	    sv_catpvn(retval, blessstr, blesslen);
	    sv_catpvs(retval, "( ");
            if (style->indent >= 2) {
		blesspad = apad;
		apad = newSVsv(apad);
		sv_x(aTHX_ apad, " ", 1, blesslen+2);
	    }
	}

        ipad = sv_x(aTHX_ Nullsv, SvPVX_const(style->xpad), SvCUR(style->xpad), level+1);

        if (is_regex) 
        {
            STRLEN rlen;
	    SV *sv_pattern = NULL;
	    SV *sv_flags = NULL;
	    CV *re_pattern_cv;
	    const char *rval;
	    const char *rend;
	    const char *slash;

	    if ((re_pattern_cv = get_cv("re::regexp_pattern", 0))) {
	      dSP;
	      I32 count;
	      ENTER;
	      SAVETMPS;
	      PUSHMARK(SP);
	      XPUSHs(val);
	      PUTBACK;
	      count = call_sv((SV*)re_pattern_cv, G_ARRAY);
	      SPAGAIN;
	      if (count >= 2) {
		sv_flags = POPs;
	        sv_pattern = POPs;
		SvREFCNT_inc(sv_flags);
		SvREFCNT_inc(sv_pattern);
	      }
	      PUTBACK;
	      FREETMPS;
	      LEAVE;
	      if (sv_pattern) {
	        sv_2mortal(sv_pattern);
	        sv_2mortal(sv_flags);
	      }
	    }
	    else {
	      sv_pattern = val;
	    }
	    assert(sv_pattern);
	    rval = SvPV(sv_pattern, rlen);
	    rend = rval+rlen;
	    slash = rval;
	    sv_catpvs(retval, "qr/");
	    for (;slash < rend; slash++) {
	      if (*slash == '\\') { ++slash; continue; }
	      if (*slash == '/') {    
		sv_catpvn(retval, rval, slash-rval);
		sv_catpvs(retval, "\\/");
		rlen -= slash-rval+1;
		rval = slash+1;
	      }
	    }
	    sv_catpvn(retval, rval, rlen);
	    sv_catpvs(retval, "/");
	    if (sv_flags)
	      sv_catsv(retval, sv_flags);
	} 
        else if (
#if PERL_VERSION < 9
		realtype <= SVt_PVBM
#else
		realtype <= SVt_PVMG
#endif
	) {			     /* scalar ref */
	    SV * const namesv = newSVpvs("${");
	    sv_catpvn(namesv, name, namelen);
	    sv_catpvs(namesv, "}");
	    if (realpack) {				     /* blessed */
		sv_catpvs(retval, "do{\\(my $o = ");
		DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
			postav, level+1, apad, style);
		sv_catpvs(retval, ")}");
	    }						     /* plain */
	    else {
		sv_catpvs(retval, "\\");
		DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
			postav, level+1, apad, style);
	    }
	    SvREFCNT_dec(namesv);
	}
	else if (realtype == SVt_PVGV) {		     /* glob ref */
	    SV * const namesv = newSVpvs("*{");
	    sv_catpvn(namesv, name, namelen);
	    sv_catpvs(namesv, "}");
	    sv_catpvs(retval, "\\");
	    DD_dump(aTHX_ ival, SvPVX_const(namesv), SvCUR(namesv), retval, seenhv,
		    postav, level+1, apad, style);
	    SvREFCNT_dec(namesv);
	}
	else if (realtype == SVt_PVAV) {
	    SV *totpad;
	    SSize_t ix = 0;
	    const SSize_t ixmax = av_len((AV *)ival);
	
	    SV * const ixsv = newSViv(0);
	    /* allowing for a 24 char wide array index */
	    New(0, iname, namelen+28, char);
	    (void) strlcpy(iname, name, namelen+28);
	    inamelen = namelen;
	    if (name[0] == '@') {
		sv_catpvs(retval, "(");
		iname[0] = '$';
	    }
	    else {
		sv_catpvs(retval, "[");
		/* omit "->" in $foo{bar}->[0], but not in ${$foo}->[0] */
		/*if (namelen > 0
		    && name[namelen-1] != ']' && name[namelen-1] != '}'
		    && (namelen < 4 || (name[1] != '{' && name[2] != '{')))*/
		if ((namelen > 0
		     && name[namelen-1] != ']' && name[namelen-1] != '}')
		    || (namelen > 4
		        && (name[1] == '{'
			    || (name[0] == '\\' && name[2] == '{'))))
		{
		    iname[inamelen++] = '-'; iname[inamelen++] = '>';
		    iname[inamelen] = '\0';
		}
	    }
	    if (iname[0] == '*' && iname[inamelen-1] == '}' && inamelen >= 8 &&
		(instr(iname+inamelen-8, "{SCALAR}") ||
		 instr(iname+inamelen-7, "{ARRAY}") ||
		 instr(iname+inamelen-6, "{HASH}"))) {
		iname[inamelen++] = '-'; iname[inamelen++] = '>';
	    }
	    iname[inamelen++] = '['; iname[inamelen] = '\0';
            totpad = newSVsv(style->sep);
            sv_catsv(totpad, style->pad);
	    sv_catsv(totpad, apad);

	    for (ix = 0; ix <= ixmax; ++ix) {
		STRLEN ilen;
		SV *elem;
		svp = av_fetch((AV*)ival, ix, FALSE);
		if (svp)
		    elem = *svp;
		else
		    elem = &PL_sv_undef;
		
		ilen = inamelen;
		sv_setiv(ixsv, ix);
#if PERL_VERSION < 10
                (void) sprintf(iname+ilen, "%" IVdf, (IV)ix);
		ilen = strlen(iname);
#else
                ilen = ilen + my_sprintf(iname+ilen, "%" IVdf, (IV)ix);
#endif
		iname[ilen++] = ']'; iname[ilen] = '\0';
                if (style->indent >= 3) {
		    sv_catsv(retval, totpad);
		    sv_catsv(retval, ipad);
		    sv_catpvs(retval, "#");
		    sv_catsv(retval, ixsv);
		}
		sv_catsv(retval, totpad);
		sv_catsv(retval, ipad);
		DD_dump(aTHX_ elem, iname, ilen, retval, seenhv, postav,
			level+1, apad, style);
		if (ix < ixmax || (style->trailingcomma && style->indent >= 1))
		    sv_catpvs(retval, ",");
	    }
	    if (ixmax >= 0) {
                SV * const opad = sv_x(aTHX_ Nullsv, SvPVX_const(style->xpad), SvCUR(style->xpad), level);
		sv_catsv(retval, totpad);
		sv_catsv(retval, opad);
		SvREFCNT_dec(opad);
	    }
	    if (name[0] == '@')
		sv_catpvs(retval, ")");
	    else
		sv_catpvs(retval, "]");
	    SvREFCNT_dec(ixsv);
	    SvREFCNT_dec(totpad);
	    Safefree(iname);
	}
	else if (realtype == SVt_PVHV) {
	    SV *totpad, *newapad;
	    SV *sname;
	    HE *entry = NULL;
	    char *key;
	    SV *hval;
	    AV *keys = NULL;
	
	    SV * const iname = newSVpvn(name, namelen);
	    if (name[0] == '%') {
		sv_catpvs(retval, "(");
		(SvPVX(iname))[0] = '$';
	    }
	    else {
		sv_catpvs(retval, "{");
		/* omit "->" in $foo[0]->{bar}, but not in ${$foo}->{bar} */
		if ((namelen > 0
		     && name[namelen-1] != ']' && name[namelen-1] != '}')
		    || (namelen > 4
		        && (name[1] == '{'
			    || (name[0] == '\\' && name[2] == '{'))))
		{
		    sv_catpvs(iname, "->");
		}
	    }
	    if (name[0] == '*' && name[namelen-1] == '}' && namelen >= 8 &&
		(instr(name+namelen-8, "{SCALAR}") ||
		 instr(name+namelen-7, "{ARRAY}") ||
		 instr(name+namelen-6, "{HASH}"))) {
		sv_catpvs(iname, "->");
	    }
	    sv_catpvs(iname, "{");
            totpad = newSVsv(style->sep);
            sv_catsv(totpad, style->pad);
	    sv_catsv(totpad, apad);
	
	    /* If requested, get a sorted/filtered array of hash keys */
	    if (style->sortkeys) {
#if PERL_VERSION >= 8
		if (style->sortkeys == &PL_sv_yes) {
		    keys = newAV();
		    (void)hv_iterinit((HV*)ival);
		    while ((entry = hv_iternext((HV*)ival))) {
			sv = hv_iterkeysv(entry);
			(void)SvREFCNT_inc(sv);
			av_push(keys, sv);
		    }
# ifdef USE_LOCALE_COLLATE
#       ifdef IN_LC     /* Use this if available */
                    if (IN_LC(LC_COLLATE))
#       else
                    if (IN_LOCALE)
#       endif
                    {
                        sortsv(AvARRAY(keys),
			   av_len(keys)+1,
                           Perl_sv_cmp_locale);
                    }
                    else
# endif
                    {
                        sortsv(AvARRAY(keys),
			   av_len(keys)+1,
                           Perl_sv_cmp);
                    }
		}
                else
#endif
		{
		    dSP; ENTER; SAVETMPS; PUSHMARK(sp);
		    XPUSHs(sv_2mortal(newRV_inc(ival))); PUTBACK;
		    i = perl_call_sv(style->sortkeys, G_SCALAR | G_EVAL);
		    SPAGAIN;
		    if (i) {
			sv = POPs;
			if (SvROK(sv) && (SvTYPE(SvRV(sv)) == SVt_PVAV))
			    keys = (AV*)SvREFCNT_inc(SvRV(sv));
		    }
		    if (! keys)
			warn("Sortkeys subroutine did not return ARRAYREF\n");
		    PUTBACK; FREETMPS; LEAVE;
		}
		if (keys)
		    sv_2mortal((SV*)keys);
	    }
	    else
		(void)hv_iterinit((HV*)ival);

            /* foreach (keys %hash) */
            for (i = 0; 1; i++) {
		char *nkey;
                char *nkey_buffer = NULL;
                STRLEN nticks = 0;
		SV* keysv;
                STRLEN klen;
		STRLEN keylen;
                STRLEN nlen;
		bool do_utf8 = FALSE;

               if (style->sortkeys) {
                   if (!(keys && (SSize_t)i <= av_len(keys))) break;
               } else {
                   if (!(entry = hv_iternext((HV *)ival))) break;
               }

		if (i)
		    sv_catpvs(retval, ",");

		if (style->sortkeys) {
		    char *key;
		    svp = av_fetch(keys, i, FALSE);
		    keysv = svp ? *svp : sv_newmortal();
		    key = SvPV(keysv, keylen);
		    svp = hv_fetch((HV*)ival, key,
                                   SvUTF8(keysv) ? -(I32)keylen : (I32)keylen, 0);
		    hval = svp ? *svp : sv_newmortal();
		}
		else {
		    keysv = hv_iterkeysv(entry);
		    hval = hv_iterval((HV*)ival, entry);
		}

		key = SvPV(keysv, keylen);
		do_utf8 = DO_UTF8(keysv);
		klen = keylen;

                sv_catsv(retval, totpad);
                sv_catsv(retval, ipad);
                /* The (very)
                   old logic was first to check utf8 flag, and if utf8 always
                   call esc_q_utf8.  This caused test to break under -Mutf8,
                   because there even strings like 'c' have utf8 flag on.
                   Hence with quotekeys == 0 the XS code would still '' quote
                   them based on flags, whereas the perl code would not,
                   based on regexps.

                   The old logic checked that the string was a valid
                   perl glob name (foo::bar), which isn't safe under
                   strict, and differs from the perl code which only
                   accepts simple identifiers.

                   With the fix for [perl #120384] I chose to make
                   their handling of key quoting compatible between XS
                   and perl.
                 */
                if (style->quotekeys || key_needs_quote(key,keylen)) {
                    if (do_utf8 || style->useqq) {
                        STRLEN ocur = SvCUR(retval);
                        klen = nlen = esc_q_utf8(aTHX_ retval, key, klen, do_utf8, style->useqq);
                        nkey = SvPVX(retval) + ocur;
                    }
                    else {
		        nticks = num_q(key, klen);
			New(0, nkey_buffer, klen+nticks+3, char);
                        nkey = nkey_buffer;
			nkey[0] = '\'';
			if (nticks)
			    klen += esc_q(nkey+1, key, klen);
			else
			    (void)Copy(key, nkey+1, klen, char);
			nkey[++klen] = '\'';
			nkey[++klen] = '\0';
                        nlen = klen;
                        sv_catpvn(retval, nkey, klen);
		    }
                }
                else {
                    nkey = key;
                    nlen = klen;
                    sv_catpvn(retval, nkey, klen);
		}
                sname = newSVsv(iname);
                sv_catpvn(sname, nkey, nlen);
                sv_catpvs(sname, "}");

                sv_catsv(retval, style->pair);
                if (style->indent >= 2) {
		    char *extra;
                    STRLEN elen = 0;
		    newapad = newSVsv(apad);
		    New(0, extra, klen+4+1, char);
		    while (elen < (klen+4))
			extra[elen++] = ' ';
		    extra[elen] = '\0';
		    sv_catpvn(newapad, extra, elen);
		    Safefree(extra);
		}
		else
		    newapad = apad;

		DD_dump(aTHX_ hval, SvPVX_const(sname), SvCUR(sname), retval, seenhv,
			postav, level+1, newapad, style);
		SvREFCNT_dec(sname);
		Safefree(nkey_buffer);
                if (style->indent >= 2)
		    SvREFCNT_dec(newapad);
	    }
	    if (i) {
                SV *opad = sv_x(aTHX_ Nullsv, SvPVX_const(style->xpad),
                                SvCUR(style->xpad), level);
                if (style->trailingcomma && style->indent >= 1)
                    sv_catpvs(retval, ",");
		sv_catsv(retval, totpad);
		sv_catsv(retval, opad);
		SvREFCNT_dec(opad);
	    }
	    if (name[0] == '%')
		sv_catpvs(retval, ")");
	    else
		sv_catpvs(retval, "}");
	    SvREFCNT_dec(iname);
	    SvREFCNT_dec(totpad);
	}
	else if (realtype == SVt_PVCV) {
            if (style->deparse) {
                SV *deparsed = sv_2mortal(deparsed_output(aTHX_ val));
                SV *fullpad = sv_2mortal(newSVsv(style->sep));
                const char *p;
                STRLEN plen;
                I32 i;

                sv_catsv(fullpad, style->pad);
                sv_catsv(fullpad, apad);
                for (i = 0; i < level; i++) {
                    sv_catsv(fullpad, style->xpad);
                }

                sv_catpvs(retval, "sub ");
                p = SvPV(deparsed, plen);
                while (plen > 0) {
                    const char *nl = (const char *) memchr(p, '\n', plen);
                    if (!nl) {
                        sv_catpvn(retval, p, plen);
                        break;
                    }
                    else {
                        size_t n = nl - p;
                        sv_catpvn(retval, p, n);
                        sv_catsv(retval, fullpad);
                        p += n + 1;
                        plen -= n + 1;
                    }
                }
            }
            else {
                sv_catpvs(retval, "sub { \"DUMMY\" }");
                if (style->purity)
                    warn("Encountered CODE ref, using dummy placeholder");
            }
	}
	else {
	    warn("cannot handle ref type %d", (int)realtype);
	}

	if (realpack && !no_bless) {  /* free blessed allocs */
            STRLEN plen, pticks;

            if (style->indent >= 2) {
		SvREFCNT_dec(apad);
		apad = blesspad;
	    }
	    sv_catpvs(retval, ", '");

	    plen = strlen(realpack);
	    pticks = num_q(realpack, plen);
	    if (pticks) { /* needs escaping */
	        char *npack;
	        char *npack_buffer = NULL;

	        New(0, npack_buffer, plen+pticks+1, char);
	        npack = npack_buffer;
	        plen += esc_q(npack, realpack, plen);
	        npack[plen] = '\0';

	        sv_catpvn(retval, npack, plen);
	        Safefree(npack_buffer);
	    }
	    else {
	        sv_catpvn(retval, realpack, strlen(realpack));
	    }
	    sv_catpvs(retval, "' )");
            if (style->toaster && SvPOK(style->toaster) && SvCUR(style->toaster)) {
		sv_catpvs(retval, "->");
                sv_catsv(retval, style->toaster);
		sv_catpvs(retval, "()");
	    }
	}
	SvREFCNT_dec(ipad);
    }
    else {
	STRLEN i;
	const MAGIC *mg;
	
	if (namelen) {
#ifdef DD_USE_OLD_ID_FORMAT
	    idlen = my_snprintf(id, sizeof(id), "0x%" UVxf, PTR2UV(val));
#else
	    id_buffer = PTR2UV(val);
	    idlen = sizeof(id_buffer);
#endif
	    if ((svp = hv_fetch(seenhv, id, idlen, FALSE)) &&
		(sv = *svp) && SvROK(sv) &&
		(seenentry = (AV*)SvRV(sv)))
	    {
		SV *othername;
		if ((svp = av_fetch(seenentry, 0, FALSE)) && (othername = *svp)
		    && (svp = av_fetch(seenentry, 2, FALSE)) && *svp && SvIV(*svp) > 0)
		{
		    sv_catpvs(retval, "${");
		    sv_catsv(retval, othername);
		    sv_catpvs(retval, "}");
		    return 1;
		}
	    }
            /* If we're allowed to keep only a sparse "seen" hash
             * (IOW, the user does not expect it to contain everything
             * after the dump, then only store in seen hash if the SV
             * ref count is larger than 1. If it's 1, then we know that
             * there is no other reference, duh. This is an optimization.
             * Note that we'd have to check for weak-refs, too, but this is
             * already the branch for non-refs only. */
            else if (val != &PL_sv_undef && (!style->use_sparse_seen_hash || SvREFCNT(val) > 1)) {
		SV * const namesv = newSVpvs("\\");
		sv_catpvn(namesv, name, namelen);
		seenentry = newAV();
		av_push(seenentry, namesv);
		av_push(seenentry, newRV_inc(val));
		(void)hv_store(seenhv, id, idlen, newRV_inc((SV*)seenentry), 0);
		SvREFCNT_dec(seenentry);
	    }
	}

        if (DD_is_integer(val)) {
            STRLEN len;
	    if (SvIsUV(val))
	      len = my_snprintf(tmpbuf, sizeof(tmpbuf), "%" UVuf, SvUV(val));
	    else
	      len = my_snprintf(tmpbuf, sizeof(tmpbuf), "%" IVdf, SvIV(val));
            if (SvPOK(val)) {
              /* Need to check to see if this is a string such as " 0".
                 I'm assuming from sprintf isn't going to clash with utf8. */
              STRLEN pvlen;
              const char * const pv = SvPV(val, pvlen);
              if (pvlen != len || memNE(pv, tmpbuf, len))
                goto integer_came_from_string;
            }
            if (len > 10) {
              /* Looks like we're on a 64 bit system.  Make it a string so that
                 if a 32 bit system reads the number it will cope better.  */
              sv_catpvf(retval, "'%s'", tmpbuf);
            } else
              sv_catpvn(retval, tmpbuf, len);
	}
	else if (realtype == SVt_PVGV) {/* GLOBs can end up with scribbly names */
	    c = SvPV(val, i);
	    if(i) ++c, --i;			/* just get the name */
	    if (memBEGINs(c, i, "main::")) {
		c += 4;
#if PERL_VERSION < 7
		if (i == 6 || (i == 7 && c[6] == '\0'))
#else
		if (i == 6)
#endif
		    i = 0; else i -= 4;
	    }
            if (globname_needs_quote(c,i)) {
		sv_grow(retval, SvCUR(retval)+3);
		r = SvPVX(retval)+SvCUR(retval);
		r[0] = '*'; r[1] = '{'; r[2] = 0;
		SvCUR_set(retval, SvCUR(retval)+2);
                i = 3 + esc_q_utf8(aTHX_ retval, c, i,
#ifdef GvNAMEUTF8
			!!GvNAMEUTF8(val)
#else
			0
#endif
			, style->useqq);
		sv_grow(retval, SvCUR(retval)+2);
		r = SvPVX(retval)+SvCUR(retval);
		r[0] = '}'; r[1] = '\0';
		SvCUR_set(retval, SvCUR(retval)+1);
		r = r+1 - i;
	    }
	    else {
		sv_grow(retval, SvCUR(retval)+i+2);
		r = SvPVX(retval)+SvCUR(retval);
		r[0] = '*'; strlcpy(r+1, c, SvLEN(retval));
		i++;
		SvCUR_set(retval, SvCUR(retval)+i);
	    }

            if (style->purity) {
		static const char* const entries[] = { "{SCALAR}", "{ARRAY}", "{HASH}" };
		static const STRLEN sizes[] = { 8, 7, 6 };
		SV *e;
		SV * const nname = newSVpvs("");
		SV * const newapad = newSVpvs("");
		GV * const gv = (GV*)val;
		I32 j;
		
		for (j=0; j<3; j++) {
		    e = ((j == 0) ? GvSV(gv) : (j == 1) ? (SV*)GvAV(gv) : (SV*)GvHV(gv));
		    if (!e)
			continue;
		    if (j == 0 && !SvOK(e))
			continue;

		    {
			SV *postentry = newSVpvn(r,i);
			
			sv_setsv(nname, postentry);
			sv_catpvn(nname, entries[j], sizes[j]);
			sv_catpvs(postentry, " = ");
			av_push(postav, postentry);
			e = newRV_inc(e);
			
			SvCUR_set(newapad, 0);
                        if (style->indent >= 2)
			    (void)sv_x(aTHX_ newapad, " ", 1, SvCUR(postentry));
			
			DD_dump(aTHX_ e, SvPVX_const(nname), SvCUR(nname), postentry,
				seenhv, postav, 0, newapad, style);
			SvREFCNT_dec(e);
		    }
		}
		
		SvREFCNT_dec(newapad);
		SvREFCNT_dec(nname);
	    }
	}
	else if (val == &PL_sv_undef || !SvOK(val)) {
	    sv_catpvs(retval, "undef");
	}
#ifdef SvVOK
	else if (SvMAGICAL(val) && (mg = mg_find(val, 'V'))) {
# if !defined(PL_vtbl_vstring) && PERL_VERSION < 17
	    SV * const vecsv = sv_newmortal();
#  if PERL_VERSION < 10
	    scan_vstring(mg->mg_ptr, vecsv);
#  else
	    scan_vstring(mg->mg_ptr, mg->mg_ptr + mg->mg_len, vecsv);
#  endif
	    if (!sv_eq(vecsv, val)) goto integer_came_from_string;
# endif
	    sv_catpvn(retval, (const char *)mg->mg_ptr, mg->mg_len);
	}
#endif

	else {
        integer_came_from_string:
            c = SvPV(val, i);
            /* the pure perl and XS non-qq outputs have historically been
             * different in this case, but for useqq, let's try to match
             * the pure perl code.
             * see [perl #74798]
             */
            if (style->useqq && safe_decimal_number(c, i)) {
                sv_catsv(retval, val);
            }
            else if (DO_UTF8(val) || style->useqq)
                i += esc_q_utf8(aTHX_ retval, c, i, DO_UTF8(val), style->useqq);
	    else {
		sv_grow(retval, SvCUR(retval)+3+2*i); /* 3: ""\0 */
		r = SvPVX(retval) + SvCUR(retval);
		r[0] = '\'';
		i += esc_q(r+1, c, i);
		++i;
		r[i++] = '\'';
		r[i] = '\0';
		SvCUR_set(retval, SvCUR(retval)+i);
	    }
	}
    }

    if (idlen) {
        if (style->deepcopy)
	    (void)hv_delete(seenhv, id, idlen, G_DISCARD);
	else if (namelen && seenentry) {
	    SV *mark = *av_fetch(seenentry, 2, TRUE);
	    sv_setiv(mark,1);
	}
    }
    return 1;
}


MODULE = Data::Dumper		PACKAGE = Data::Dumper         PREFIX = Data_Dumper_

#
# This is the exact equivalent of Dump.  Well, almost. The things that are
# different as of now (due to Laziness):
#   * doesn't do deparse yet.'
#

void
Data_Dumper_Dumpxs(href, ...)
	SV	*href;
	PROTOTYPE: $;$$
	PPCODE:
	{
	    HV *hv;
	    SV *retval, *valstr;
	    HV *seenhv = NULL;
	    AV *postav, *todumpav, *namesav;
	    I32 terse = 0;
	    SSize_t i, imax, postlen;
	    SV **svp;
            SV *apad = &PL_sv_undef;
            Style style;

            SV *name, *val = &PL_sv_undef, *varname = &PL_sv_undef;
	    char tmpbuf[1024];
	    I32 gimme = GIMME_V;

	    if (!SvROK(href)) {		/* call new to get an object first */
		if (items < 2)
		    croak("Usage: Data::Dumper::Dumpxs(PACKAGE, VAL_ARY_REF, [NAME_ARY_REF])");
		
		ENTER;
		SAVETMPS;
		
		PUSHMARK(sp);
                EXTEND(SP, 3); /* 3 == max of all branches below */
		PUSHs(href);
		PUSHs(sv_2mortal(newSVsv(ST(1))));
		if (items >= 3)
		    PUSHs(sv_2mortal(newSVsv(ST(2))));
		PUTBACK;
		i = perl_call_method("new", G_SCALAR);
		SPAGAIN;
		if (i)
		    href = newSVsv(POPs);

		PUTBACK;
		FREETMPS;
		LEAVE;
		if (i)
		    (void)sv_2mortal(href);
	    }

	    todumpav = namesav = NULL;
            style.indent = 2;
            style.quotekeys = 1;
            style.maxrecurse = 1000;
            style.purity = style.deepcopy = style.useqq = style.maxdepth
                = style.use_sparse_seen_hash = style.trailingcomma = 0;
            style.pad = style.xpad = style.sep = style.pair = style.sortkeys
                = style.freezer = style.toaster = style.bless = &PL_sv_undef;
	    seenhv = NULL;
	    name = sv_newmortal();
	
	    retval = newSVpvs("");
	    if (SvROK(href)
		&& (hv = (HV*)SvRV((SV*)href))
		&& SvTYPE(hv) == SVt_PVHV)		{

		if ((svp = hv_fetchs(hv, "seen", FALSE)) && SvROK(*svp))
		    seenhv = (HV*)SvRV(*svp);
                else
                    style.use_sparse_seen_hash = 1;
		if ((svp = hv_fetchs(hv, "noseen", FALSE)))
                    style.use_sparse_seen_hash = (SvOK(*svp) && SvIV(*svp) != 0);
		if ((svp = hv_fetchs(hv, "todump", FALSE)) && SvROK(*svp))
		    todumpav = (AV*)SvRV(*svp);
		if ((svp = hv_fetchs(hv, "names", FALSE)) && SvROK(*svp))
		    namesav = (AV*)SvRV(*svp);
		if ((svp = hv_fetchs(hv, "indent", FALSE)))
                    style.indent = SvIV(*svp);
		if ((svp = hv_fetchs(hv, "purity", FALSE)))
                    style.purity = SvIV(*svp);
		if ((svp = hv_fetchs(hv, "terse", FALSE)))
		    terse = SvTRUE(*svp);
		if ((svp = hv_fetchs(hv, "useqq", FALSE)))
                    style.useqq = SvTRUE(*svp);
		if ((svp = hv_fetchs(hv, "pad", FALSE)))
                    style.pad = *svp;
		if ((svp = hv_fetchs(hv, "xpad", FALSE)))
                    style.xpad = *svp;
		if ((svp = hv_fetchs(hv, "apad", FALSE)))
		    apad = *svp;
		if ((svp = hv_fetchs(hv, "sep", FALSE)))
                    style.sep = *svp;
		if ((svp = hv_fetchs(hv, "pair", FALSE)))
                    style.pair = *svp;
		if ((svp = hv_fetchs(hv, "varname", FALSE)))
		    varname = *svp;
		if ((svp = hv_fetchs(hv, "freezer", FALSE)))
                    style.freezer = *svp;
		if ((svp = hv_fetchs(hv, "toaster", FALSE)))
                    style.toaster = *svp;
		if ((svp = hv_fetchs(hv, "deepcopy", FALSE)))
                    style.deepcopy = SvTRUE(*svp);
		if ((svp = hv_fetchs(hv, "quotekeys", FALSE)))
                    style.quotekeys = SvTRUE(*svp);
                if ((svp = hv_fetchs(hv, "trailingcomma", FALSE)))
                    style.trailingcomma = SvTRUE(*svp);
                if ((svp = hv_fetchs(hv, "deparse", FALSE)))
                    style.deparse = SvTRUE(*svp);
		if ((svp = hv_fetchs(hv, "bless", FALSE)))
                    style.bless = *svp;
		if ((svp = hv_fetchs(hv, "maxdepth", FALSE)))
                    style.maxdepth = SvIV(*svp);
		if ((svp = hv_fetchs(hv, "maxrecurse", FALSE)))
                    style.maxrecurse = SvIV(*svp);
		if ((svp = hv_fetchs(hv, "sortkeys", FALSE))) {
                    SV *sv = *svp;
                    if (! SvTRUE(sv))
                        style.sortkeys = NULL;
                    else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVCV)
                        style.sortkeys = sv;
                    else if (PERL_VERSION < 8)
                        /* 5.6 doesn't make sortsv() available to XS code,
                         * so we must use this helper instead. Note that we
                         * always allocate this mortal SV, but it will be
                         * used only if at least one hash is encountered
                         * while dumping recursively; an older version
                         * allocated it lazily as needed. */
                        style.sortkeys = sv_2mortal(newSVpvs("Data::Dumper::_sortkeys"));
                    else
                        /* flag to use sortsv() for sorting hash keys */
                        style.sortkeys = &PL_sv_yes;
		}
		postav = newAV();

		if (todumpav)
		    imax = av_len(todumpav);
		else
		    imax = -1;
		valstr = newSVpvs("");
		for (i = 0; i <= imax; ++i) {
		    SV *newapad;
		
		    av_clear(postav);
		    if ((svp = av_fetch(todumpav, i, FALSE)))
			val = *svp;
		    else
			val = &PL_sv_undef;
		    if ((svp = av_fetch(namesav, i, TRUE))) {
			sv_setsv(name, *svp);
			if (SvOK(*svp) && !SvPOK(*svp))
			    (void)SvPV_nolen_const(name);
		    }
		    else
			(void)SvOK_off(name);
		
		    if (SvPOK(name)) {
			if ((SvPVX_const(name))[0] == '*') {
			    if (SvROK(val)) {
				switch (SvTYPE(SvRV(val))) {
				case SVt_PVAV:
				    (SvPVX(name))[0] = '@';
				    break;
				case SVt_PVHV:
				    (SvPVX(name))[0] = '%';
				    break;
				case SVt_PVCV:
				    (SvPVX(name))[0] = '*';
				    break;
				default:
				    (SvPVX(name))[0] = '$';
				    break;
				}
			    }
			    else
				(SvPVX(name))[0] = '$';
			}
			else if ((SvPVX_const(name))[0] != '$')
			    sv_insert(name, 0, 0, "$", 1);
		    }
		    else {
			STRLEN nchars;
			sv_setpvs(name, "$");
			sv_catsv(name, varname);
			nchars = my_snprintf(tmpbuf, sizeof(tmpbuf), "%" IVdf,
                                                                     (IV)(i+1));
			sv_catpvn(name, tmpbuf, nchars);
		    }
		
                    if (style.indent >= 2 && !terse) {
			SV * const tmpsv = sv_x(aTHX_ NULL, " ", 1, SvCUR(name)+3);
			newapad = newSVsv(apad);
			sv_catsv(newapad, tmpsv);
			SvREFCNT_dec(tmpsv);
		    }
		    else
			newapad = apad;
		
		    PUTBACK;
		    DD_dump(aTHX_ val, SvPVX_const(name), SvCUR(name), valstr, seenhv,
                            postav, 0, newapad, &style);
		    SPAGAIN;
		
                    if (style.indent >= 2 && !terse)
			SvREFCNT_dec(newapad);

		    postlen = av_len(postav);
		    if (postlen >= 0 || !terse) {
			sv_insert(valstr, 0, 0, " = ", 3);
			sv_insert(valstr, 0, 0, SvPVX_const(name), SvCUR(name));
			sv_catpvs(valstr, ";");
		    }
                    sv_catsv(retval, style.pad);
		    sv_catsv(retval, valstr);
                    sv_catsv(retval, style.sep);
		    if (postlen >= 0) {
			SSize_t i;
                        sv_catsv(retval, style.pad);
			for (i = 0; i <= postlen; ++i) {
			    SV *elem;
			    svp = av_fetch(postav, i, FALSE);
			    if (svp && (elem = *svp)) {
				sv_catsv(retval, elem);
				if (i < postlen) {
				    sv_catpvs(retval, ";");
                                    sv_catsv(retval, style.sep);
                                    sv_catsv(retval, style.pad);
				}
			    }
			}
			sv_catpvs(retval, ";");
                        sv_catsv(retval, style.sep);
		    }
		    SvPVCLEAR(valstr);
		    if (gimme == G_ARRAY) {
			XPUSHs(sv_2mortal(retval));
			if (i < imax)	/* not the last time thro ? */
			    retval = newSVpvs("");
		    }
		}
		SvREFCNT_dec(postav);
		SvREFCNT_dec(valstr);
	    }
	    else
		croak("Call to new() method failed to return HASH ref");
	    if (gimme != G_ARRAY)
		XPUSHs(sv_2mortal(retval));
	}

SV *
Data_Dumper__vstring(sv)
	SV	*sv;
	PROTOTYPE: $
	CODE:
	{
#ifdef SvVOK
	    const MAGIC *mg;
	    RETVAL =
		SvMAGICAL(sv) && (mg = mg_find(sv, 'V'))
		 ? newSVpvn((const char *)mg->mg_ptr, mg->mg_len)
		 : &PL_sv_undef;
#else
	    RETVAL = &PL_sv_undef;
#endif
	}
	OUTPUT: RETVAL
